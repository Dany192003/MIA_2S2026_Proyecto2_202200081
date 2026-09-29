#include "command_handler.h"
#include "../utils/ext2_utils.h"
#include <fstream>
#include <sstream>
#include <cstring>

CommandResult CommandHandler::processLogin(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        std::string user = params["user"];
        std::string pass = params["pass"];
        std::string id = params["id"];
        
        //Verificar sesión activa
        if (currentSession.active) {
            result.message = "Error: Ya hay una sesión activa. Use LOGOUT primero.";
            return result;
        }
        
        if (mountedDisks.find(id) == mountedDisks.end()) {
            result.message = "Error: El ID de montaje no existe: " + id;
            return result;
        }
        
        std::string diskPath = mountedDisks[id];
        
        std::fstream disk(diskPath, std::ios::in | std::ios::binary);
        if (!disk.is_open()) {
            result.message = "Error: No se pudo abrir el disco: " + diskPath;
            return result;
        }
        
        MBR mbr;
        disk.seekg(0, std::ios::beg);
        disk.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        
        int partitionIndex = -1;
        for (int i = 0; i < 4; i++) {
            char partId[5] = {0};
            memcpy(partId, mbr.mbr_partitions[i].part_id, 4);
            partId[4] = '\0';
            std::string partIdStr(partId);
            partIdStr = partIdStr.c_str();
            
            if (partIdStr == id && mbr.mbr_partitions[i].part_s > 0) {
                partitionIndex = i;
                break;
            }
        }
        
        if (partitionIndex == -1) {
            disk.close();
            result.message = "Error: No se encontró la partición para el ID: " + id;
            return result;
        }
        disk.close();
        
        Superblock sb = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
        std::string usersContent = Ext2Utils::readFile(diskPath, "/users.txt", sb, mbr, partitionIndex);
        
        std::vector<std::string> userLines;
        if (!usersContent.empty()) {
            std::stringstream ss(usersContent);
            std::string line;
            while (std::getline(ss, line)) {
                if (!line.empty()) {
                    userLines.push_back(line);
                }
            }
        }
        
        if (userLines.empty()) {
            userLines.push_back("1, G, root");
            userLines.push_back("1, U, root, root, 123");
        }
        
        bool userFound = false;
        int uid = -1;
        int gid = -1;
        std::string group = "";
        std::string password;
        
        for (const auto& line : userLines) {
            if (line.find(", U, ") != std::string::npos) {
                std::stringstream ss(line);
                std::string token;
                std::vector<std::string> parts;
                while (std::getline(ss, token, ',')) {
                    parts.push_back(token);
                }
                
                if (parts.size() >= 5) {
                    for (auto& p : parts) {
                        p.erase(0, p.find_first_not_of(" "));
                        p.erase(p.find_last_not_of(" ") + 1);
                    }
                    
                    if (parts[3] == user && std::stoi(parts[0]) != 0) {
                        userFound = true;
                        uid = std::stoi(parts[0]);
                        group = parts[2];
                        password = parts[4];
                        
                        for (const auto& gline : userLines) {
                            if (gline.find(", G, ") != std::string::npos) {
                                std::stringstream gss(gline);
                                std::string gtoken;
                                std::vector<std::string> gparts;
                                while (std::getline(gss, gtoken, ',')) {
                                    gparts.push_back(gtoken);
                                }
                                if (gparts.size() >= 3) {
                                    for (auto& p : gparts) {
                                        p.erase(0, p.find_first_not_of(" "));
                                        p.erase(p.find_last_not_of(" ") + 1);
                                    }
                                    if (gparts[2] == group && std::stoi(gparts[0]) != 0) {
                                        gid = std::stoi(gparts[0]);
                                        break;
                                    }
                                }
                            }
                        }
                        break;
                    }
                }
            }
        }
        
        if (!userFound) {
            result.message = "Error: Usuario no encontrado: " + user;
            return result;
        }
        
        if (password != pass) {
            result.message = "Error: Autenticación fallida";
            return result;
        }
        
        currentSession.active = true;
        currentSession.user = user;
        currentSession.mountId = id;
        currentSession.diskPath = diskPath;
        currentSession.uid = uid;
        currentSession.gid = gid;
        currentSession.group = group;
        
        result.success = true;
        result.message = "Sesión iniciada como: " + user;
        result.data["session"] = {
            {"user", user},
            {"id", id},
            {"uid", currentSession.uid},
            {"gid", currentSession.gid},
            {"group", currentSession.group}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar LOGIN: " + std::string(e.what());
    }
    
    return result;
}