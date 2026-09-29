#include "command_handler.h"
#include "../utils/ext2_utils.h"
#include <fstream>
#include <sstream>
#include <cstring>

CommandResult CommandHandler::processChgrp(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        // 1. Obtener parámetros
        std::string user = params["user"];
        std::string grp = params["grp"];
        
        // 2. Validar que haya sesión activa
        if (!isLoggedIn()) {
            result.message = "Error: No hay sesión activa. Use LOGIN primero.";
            return result;
        }
        
        // 3. Validar que sea usuario root
        if (!isRoot()) {
            result.message = "Error: Solo el usuario root puede cambiar grupos";
            return result;
        }
        
        // 4. Validar que no sea el usuario root
        if (user == "root") {
            result.message = "Error: No se puede cambiar el grupo del usuario root";
            return result;
        }
        
        // 5. Validar nombres (máximo 10 caracteres)
        if (user.length() > 10 || grp.length() > 10) {
            result.message = "Error: Los nombres no pueden exceder 10 caracteres";
            return result;
        }
        
        // 6. Obtener la ruta del disco y el ID de montaje
        std::string diskPath = currentSession.diskPath;
        std::string mountId = currentSession.mountId;
        
        // 7. Leer el MBR
        std::fstream disk(diskPath, std::ios::in | std::ios::out | std::ios::binary);
        if (!disk.is_open()) {
            result.message = "Error: No se pudo abrir el disco: " + diskPath;
            return result;
        }
        
        MBR mbr;
        disk.seekg(0, std::ios::beg);
        disk.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        
        // 8. Buscar la partición por ID
        int partitionIndex = -1;
        for (int i = 0; i < 4; i++) {
            char partId[5] = {0};
            memcpy(partId, mbr.mbr_partitions[i].part_id, 4);
            partId[4] = '\0';
            std::string partIdStr(partId);
            partIdStr = partIdStr.c_str();
            
            if (partIdStr == mountId && mbr.mbr_partitions[i].part_s > 0) {
                partitionIndex = i;
                break;
            }
        }
        
        if (partitionIndex == -1) {
            disk.close();
            result.message = "Error: No se encontró la partición para el ID: " + mountId;
            return result;
        }
        
        // 9. Leer users.txt
        std::vector<std::string> userLines = Ext2Utils::readUsersFile(diskPath, mountId, mbr, partitionIndex);
        
        // 10. Buscar el usuario y verificar que el grupo exista
        bool userFound = false;
        bool groupFound = false;
        int userLineIndex = -1;
        int userUid = -1;
        
        // Primero, verificar que el grupo destino exista (y no esté eliminado)
        for (const auto& line : userLines) {
            if (line.find(", G, ") != std::string::npos) {
                std::stringstream ss(line);
                std::string token;
                std::vector<std::string> parts;
                while (std::getline(ss, token, ',')) {
                    parts.push_back(token);
                }
                if (parts.size() >= 3) {
                    for (auto& p : parts) {
                        p.erase(0, p.find_first_not_of(" "));
                        p.erase(p.find_last_not_of(" ") + 1);
                    }
                    
                    std::string groupName = parts[2];
                    int gid = std::stoi(parts[0]);
                    
                    if (groupName == grp && gid != 0) {
                        groupFound = true;
                        break;
                    }
                }
            }
        }
        
        if (!groupFound) {
            disk.close();
            result.message = "Error: El grupo destino no existe o está eliminado: " + grp;
            return result;
        }
        
        //buscar el usuario y su línea
        for (size_t i = 0; i < userLines.size(); i++) {
            const std::string& line = userLines[i];
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
                    
                    std::string username = parts[3];
                    int uid = std::stoi(parts[0]);
                    
                    if (username == user && uid != 0) {
                        userFound = true;
                        userLineIndex = i;
                        userUid = uid;
                        break;
                    }
                }
            }
        }
        
        if (!userFound) {
            disk.close();
            result.message = "Error: El usuario no existe o está eliminado: " + user;
            return result;
        }
        
        // 11. Modificar la línea del usuario
        std::string oldLine = userLines[userLineIndex];
        std::stringstream ss(oldLine);
        std::string token;
        std::vector<std::string> parts;
        while (std::getline(ss, token, ',')) {
            parts.push_back(token);
        }
        
        // Limpiar espacios
        for (auto& p : parts) {
            p.erase(0, p.find_first_not_of(" "));
            p.erase(p.find_last_not_of(" ") + 1);
        }
        
        // Reconstruir la línea con el nuevo grupo
        // Formato: UID, U, Grupo, Usuario, Contraseña
        std::string newLine = parts[0] + ", U, " + grp + ", " + parts[3] + ", " + parts[4];
        userLines[userLineIndex] = newLine;
        
        // 12. Guardar users.txt
        if (!Ext2Utils::writeUsersFile(diskPath, userLines, mbr, partitionIndex)) {
            disk.close();
            result.message = "Error: No se pudo guardar el cambio en users.txt";
            return result;
        }
        
        disk.close();
        
        // 13. Éxito
        result.success = true;
        result.message = "Grupo cambiado exitosamente para el usuario: " + user + " (Nuevo grupo: " + grp + ")";
        result.data["change"] = {
            {"user", user},
            {"new_group", grp},
            {"uid", userUid}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar CHGRP: " + std::string(e.what());
    }
    
    return result;
}