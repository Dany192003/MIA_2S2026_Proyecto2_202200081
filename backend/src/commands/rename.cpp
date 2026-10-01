#include "command_handler.h"
#include "../utils/ext2_utils.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <vector>

// ============================================================
// Helpers de permisos
// ============================================================

static bool hasWritePermission(const std::string& perms, const std::string& user,
                                int uid, int gid, int fileUid, int fileGid) {
    if (user == "root") return true;
    
    char writeBit;
    if (uid == fileUid) {
        writeBit = perms[0];
    } else if (gid == fileGid) {
        writeBit = perms[1];
    } else {
        writeBit = perms[2];
    }
    
    int permValue = writeBit - '0';
    return (permValue & 2) != 0;
}

// ============================================================
// Obtener GID actual del usuario
// ============================================================
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

// ============================================================
// RENAME
// ============================================================
CommandResult CommandHandler::processRename(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        // 1. Validar sesión
        if (!isLoggedIn()) {
            result.message = "Error: No hay sesión activa. Use LOGIN primero.";
            return result;
        }
        
        // 2. Validar parámetros
        if (!params.contains("path")) {
            result.message = "Error: El parámetro -path es obligatorio";
            return result;
        }
        if (!params.contains("name")) {
            result.message = "Error: El parámetro -name es obligatorio";
            return result;
        }
        
        std::string path = params["path"];
        std::string newName = params["name"];
        
        if (path.empty() || path == "/") {
            result.message = "Error: No se puede renombrar la raíz";
            return result;
        }
        
        // 3. Validar longitud del nuevo nombre (máx 11 chars)
        if (newName.length() > 11) {
            result.message = "Error: El nombre no puede exceder 11 caracteres: " + newName;
            return result;
        }
        
        // Validar que el nombre no contenga '/'
        if (newName.find('/') != std::string::npos) {
            result.message = "Error: El nombre no puede contener '/'";
            return result;
        }
        
        // 4. Obtener info de sesión
        std::string diskPath = currentSession.diskPath;
        std::string mountId = currentSession.mountId;
        int uid = currentSession.uid;
        int gid = currentSession.gid;
        std::string user = currentSession.user;
        
        // 5. Abrir disco y leer MBR
        std::fstream disk(diskPath, std::ios::in | std::ios::out | std::ios::binary);
        if (!disk.is_open()) {
            result.message = "Error: No se pudo abrir el disco: " + diskPath;
            return result;
        }
        
        MBR mbr;
        disk.seekg(0, std::ios::beg);
        disk.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        
        // 6. Buscar la partición
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
        
        // 7. Leer superbloque
        Superblock sb = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
        
        // 8. Leer users.txt para obtener GID actual
        std::vector<std::string> userLines = Ext2Utils::readUsersFile(diskPath, mountId, mbr, partitionIndex);
        int currentGid = getCurrentUserGid(userLines, user);
        if (currentGid != 0) gid = currentGid;
        
        // 9. Buscar el inodo del path
        int targetInode = Ext2Utils::findInodeByPathInternal(disk, path, sb);
        if (targetInode == -1) {
            disk.close();
            result.message = "Error: No existe la ruta: " + path;
            return result;
        }
        
        // 10. Verificar permisos de escritura sobre el objetivo
        Inode targetInodeStruct = Ext2Utils::readInode(disk, sb, targetInode);
        std::string targetPerms(targetInodeStruct.i_perm, 3);
        if (!hasWritePermission(targetPerms, user, uid, gid,
                                 targetInodeStruct.i_uid, targetInodeStruct.i_gid)) {
            disk.close();
            result.message = "Error: No tienes permisos de escritura sobre: " + path;
            return result;
        }
        
        // 11. Calcular el parentPath y el nombre actual
        std::vector<std::string> parts = Ext2Utils::splitPath(path);
        std::string oldName = parts.back();
        parts.pop_back();
        
        std::string parentPath = "/";
        if (!parts.empty()) {
            parentPath += parts[0];
            for (size_t i = 1; i < parts.size(); i++) {
                parentPath += "/" + parts[i];
            }
        }
        
        // 12. Validar que el nuevo nombre NO exista ya en el padre
        std::string newPath = parentPath;
        if (newPath.back() != '/') newPath += "/";
        newPath += newName;
        
        int existingInode = Ext2Utils::findInodeByPathInternal(disk, newPath, sb);
        if (existingInode != -1) {
            disk.close();
            result.message = "Error: Ya existe un archivo o carpeta con el nombre: " + newName;
            return result;
        }
        
        // 13. Buscar la entrada en el bloque del padre y renombrarla
        int parentInodeIndex = Ext2Utils::findInodeByPathInternal(disk, parentPath, sb);
        if (parentInodeIndex == -1) {
            disk.close();
            result.message = "Error: No se encontró la carpeta padre: " + parentPath;
            return result;
        }
        
        Inode parentInode = Ext2Utils::readInode(disk, sb, parentInodeIndex);
        
        bool renamed = false;
        for (int b = 0; b < 12 && !renamed; b++) {
            if (parentInode.i_block[b] == -1) continue;
            
            BlockFolder block = Ext2Utils::readBlockFolder(disk, sb, parentInode.i_block[b]);
            
            for (int i = 0; i < 4; i++) {
                std::string name(block.b_content[i].b_name);
                name = name.c_str();
                
                if (name == oldName && block.b_content[i].b_inodo == targetInode) {
                    // Renombrar: limpiar el b_name y copiar el nuevo
                    memset(block.b_content[i].b_name, 0, 12);
                    strncpy(block.b_content[i].b_name, newName.c_str(), 11);
                    
                    Ext2Utils::writeBlockFolder(disk, sb, parentInode.i_block[b], block);
                    renamed = true;
                    break;
                }
            }
        }
        
        if (!renamed) {
            disk.close();
            result.message = "Error: No se pudo renombrar (entrada no encontrada en el padre)";
            return result;
        }
        
        disk.close();
        
        // ✅ NUEVO: Registrar en journal
        if (sb.s_filesystem_type == 3) {
            writeJournal(sb, partitionIndex, "rename", path, newName);
        }
        
        // 14. Éxito
        result.success = true;
        result.message = "Renombrado exitosamente: '" + oldName + "' → '" + newName + "'";
        result.data["rename"] = {
            {"old_path", path},
            {"new_path", newPath},
            {"old_name", oldName},
            {"new_name", newName}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar RENAME: " + std::string(e.what());
    }
    
    return result;
}