#include "command_handler.h"
#include "../utils/ext2_utils.h"
#include <fstream>
#include <sstream>
#include <cstring>

// Función para verificar permisos de lectura
static bool hasReadPermission(const std::string& perms, const std::string& user, int uid, int gid, int fileUid, int fileGid) {
    char readBit;
    
    // Si es root, siempre tiene permisos
    if (user == "root") return true;
    
    // Verificar categoría del usuario
    if (uid == fileUid) {
        // Es el propietario - Usar permisos de User (primer dígito)
        readBit = perms[0];
    } else if (gid == fileGid) {
        // Pertenece al mismo grupo - Usar permisos de Group (segundo dígito)
        readBit = perms[1];
    } else {
        // Otro usuario - Usar permisos de Other (tercer dígito)
        readBit = perms[2];
    }
    
    // Verificar bit de lectura (4 en octal)
    int permValue = readBit - '0';
    return (permValue & 4) != 0;
}

// Función para obtener el GID del usuario actual desde users.txt
static int getCurrentUserGid(const std::vector<std::string>& userLines, const std::string& username) {
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
                if (parts[3] == username && std::stoi(parts[0]) != 0) {
                    // El grupo está en la posición 2
                    // Buscar el GID del grupo por nombre
                    std::string groupName = parts[2];
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
                                if (gparts[2] == groupName && std::stoi(gparts[0]) != 0) {
                                    return std::stoi(gparts[0]);
                                }
                            }
                        }
                    }
                    return 0;
                }
            }
        }
    }
    return 0;
}

CommandResult CommandHandler::processCat(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        // 1. Obtener parámetros (file1, file2, file3, ...)
        std::vector<std::string> files;
        for (int i = 1; ; i++) {
            std::string key = "file" + std::to_string(i);
            if (params.contains(key)) {
                files.push_back(params[key]);
            } else {
                break;
            }
        }
        
        // 2. Validar que haya sesión activa
        if (!isLoggedIn()) {
            result.message = "Error: No hay sesión activa. Use LOGIN primero.";
            return result;
        }
        
        // 3. Validar que haya al menos un archivo
        if (files.empty()) {
            result.message = "Error: Se requiere al menos un archivo (file1, file2, ...)";
            return result;
        }
        
        // 4. Obtener información de la partición
        std::string diskPath = currentSession.diskPath;
        std::string mountId = currentSession.mountId;
        int uid = currentSession.uid;
        int gid = currentSession.gid;
        std::string user = currentSession.user;
        
        // 5. Leer el MBR
        std::fstream disk(diskPath, std::ios::in | std::ios::out | std::ios::binary);
        if (!disk.is_open()) {
            result.message = "Error: No se pudo abrir el disco: " + diskPath;
            return result;
        }
        
        MBR mbr;
        disk.seekg(0, std::ios::beg);
        disk.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        
        // 6. Buscar la partición por ID
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
        
        // 7. Leer el Superblock
        Superblock sb = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
        disk.close();
        
        // 8. Leer users.txt para obtener el GID actual si es necesario
        std::vector<std::string> userLines = Ext2Utils::readUsersFile(diskPath, mountId, mbr, partitionIndex);
        int currentGid = getCurrentUserGid(userLines, user);
        if (currentGid != 0) {
            gid = currentGid;
        }
        
        // 9. Leer cada archivo
        std::string output;
        for (const auto& filePath : files) {
            // Verificar que el archivo exista en EXT2
            int fileInode = Ext2Utils::findInodeByPath(diskPath, filePath, sb, mbr, partitionIndex);
            if (fileInode == -1) {
                result.message = "Error: El archivo no existe en EXT2: " + filePath;
                return result;
            }
            
            // Leer el inodo del archivo
            std::fstream diskRead(diskPath, std::ios::in | std::ios::binary);
            if (!diskRead.is_open()) {
                result.message = "Error: No se pudo abrir el disco para leer inodo";
                return result;
            }
            Inode inode = Ext2Utils::readInode(diskRead, sb, fileInode);
            diskRead.close();
            
            // Verificar que sea un archivo (no carpeta)
            if (inode.i_type != '1') {
                result.message = "Error: " + filePath + " es una carpeta, no un archivo";
                return result;
            }
            
            // Verificar permisos de lectura
            std::string perms(inode.i_perm, 3);
            if (!hasReadPermission(perms, user, uid, gid, inode.i_uid, inode.i_gid)) {
                result.message = "Error: Permiso denegado para leer: " + filePath + " (Permisos: " + perms + ")";
                return result;
            }
            
            // Leer el contenido del archivo
            std::string content = Ext2Utils::readFile(diskPath, filePath, sb, mbr, partitionIndex);
            
            if (!output.empty()) {
                output += "\n";
            }
            output += content;
        }
        
        // 10. Éxito
        result.success = true;
        result.message = "Contenido de " + std::to_string(files.size()) + " archivo(s)";
        result.data["content"] = output;
        result.data["files"] = files;
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar CAT: " + std::string(e.what());
    }
    
    return result;
}