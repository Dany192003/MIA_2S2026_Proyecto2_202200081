#include "command_handler.h"
#include "../utils/ext2_utils.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <vector>
#include <functional>

// ============================================================
// Obtener el UID de un usuario por nombre desde users.txt
// Retorna -1 si no existe o está eliminado
// ============================================================
static int findUserId(const std::vector<std::string>& userLines, const std::string& username) {
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
                int uid = std::stoi(parts[0]);
                if (parts[3] == username && uid != 0) {
                    return uid;
                }
            }
        }
    }
    return -1;
}

// ============================================================
// Obtener el GID de un grupo por nombre desde users.txt
// ============================================================
static int findGroupId(const std::vector<std::string>& userLines, const std::string& groupName) {
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
                int gid = std::stoi(parts[0]);
                if (parts[2] == groupName && gid != 0) {
                    return gid;
                }
            }
        }
    }
    return -1;
}

// ============================================================
// Cambiar el UID y GID de un inodo
// ============================================================
static bool changeInodeOwner(std::fstream& disk, const Superblock& sb,
                              int inodeIndex, int newUid, int newGid) {
    Inode inode = Ext2Utils::readInode(disk, sb, inodeIndex);
    if (inode.i_type != '0' && inode.i_type != '1') return false;
    
    inode.i_uid = newUid;
    inode.i_gid = newGid;
    inode.i_mtime = time(nullptr);
    
    Ext2Utils::writeInode(disk, sb, inodeIndex, inode);
    return true;
}

// ============================================================
// CHOWN recursivo (solo si -r está presente)
// ============================================================
static void chownRecursive(std::fstream& disk, const Superblock& sb,
                            int inodeIndex, int newUid, int newGid,
                            int& changedCount) {
    Inode inode = Ext2Utils::readInode(disk, sb, inodeIndex);
    
    // Cambiar propietario del inodo actual
    inode.i_uid = newUid;
    inode.i_gid = newGid;
    inode.i_mtime = time(nullptr);
    Ext2Utils::writeInode(disk, sb, inodeIndex, inode);
    changedCount++;
    
    // Si es carpeta, recursión
    if (inode.i_type != '0') return;
    
    for (int b = 0; b < 12; b++) {
        if (inode.i_block[b] == -1) continue;
        
        BlockFolder block = Ext2Utils::readBlockFolder(disk, sb, inode.i_block[b]);
        
        for (int i = 0; i < 4; i++) {
            std::string name(block.b_content[i].b_name);
            name = name.c_str();
            
            if (name.empty() || name == "." || name == "..") continue;
            if (block.b_content[i].b_inodo == -1) continue;
            
            int childInode = block.b_content[i].b_inodo;
            chownRecursive(disk, sb, childInode, newUid, newGid, changedCount);
        }
    }
}

// ============================================================
// CHOWN
// Según PDF:
// - Root puede cambiar propietario de cualquier archivo/carpeta
// - Usuarios normales solo sobre SUS PROPIOS archivos
// - -r opcional para recursivo
// ============================================================
CommandResult CommandHandler::processChown(const json& params) {
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
        if (!params.contains("usuario")) {
            result.message = "Error: El parámetro -usuario es obligatorio";
            return result;
        }
        
        std::string path = params["path"];
        std::string newUser = params["usuario"];
        bool recursive = params.contains("r") && std::string(params["r"]) == "true";
        
        if (path.empty() || path == "/") {
            result.message = "Error: No se puede cambiar el propietario de la raíz";
            return result;
        }
        
        // 3. Obtener info de sesión
        std::string diskPath = currentSession.diskPath;
        std::string mountId = currentSession.mountId;
        int uid = currentSession.uid;
        int gid = currentSession.gid;
        std::string user = currentSession.user;
        
        // 4. Abrir disco y leer MBR
        std::fstream disk(diskPath, std::ios::in | std::ios::out | std::ios::binary);
        if (!disk.is_open()) {
            result.message = "Error: No se pudo abrir el disco: " + diskPath;
            return result;
        }
        
        MBR mbr;
        disk.seekg(0, std::ios::beg);
        disk.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        
        // 5. Buscar la partición
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
        
        // 6. Leer superbloque
        Superblock sb = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
        
        // 7. Leer users.txt
        std::vector<std::string> userLines = Ext2Utils::readUsersFile(diskPath, mountId, mbr, partitionIndex);
        
        // 8. Validar que el usuario destino exista y esté activo
        int newUid = findUserId(userLines, newUser);
        if (newUid == -1) {
            disk.close();
            result.message = "Error: El usuario no existe o está eliminado: " + newUser;
            return result;
        }
        
        // 9. Obtener el GID del grupo al que pertenece el nuevo usuario
        int newGid = 1;  // default: root
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
                    if (parts[3] == newUser && std::stoi(parts[0]) != 0) {
                        int groupGid = findGroupId(userLines, parts[2]);
                        if (groupGid != -1) newGid = groupGid;
                        break;
                    }
                }
            }
        }
        
        // 10. Buscar el inodo del path
        int targetInode = Ext2Utils::findInodeByPathInternal(disk, path, sb);
        if (targetInode == -1) {
            disk.close();
            result.message = "Error: No existe la ruta: " + path;
            return result;
        }
        
        // 11. Validar permisos según PDF
        Inode targetInodeStruct = Ext2Utils::readInode(disk, sb, targetInode);
        
        // ✅ Root puede cambiar cualquier archivo
        // ✅ Usuario normal solo puede cambiar SUS PROPIOS archivos
        if (!isRoot()) {
            if (targetInodeStruct.i_uid != uid) {
                disk.close();
                result.message = "Error: Solo puedes cambiar el propietario de tus propios archivos";
                return result;
            }
        }
        
        // 12. Cambiar propietario
        int changedCount = 0;
        
        if (recursive && targetInodeStruct.i_type == '0') {
            chownRecursive(disk, sb, targetInode, newUid, newGid, changedCount);
        } else {
            changeInodeOwner(disk, sb, targetInode, newUid, newGid);
            changedCount = 1;
        }
        
        // 13. Actualizar superbloque
        Superblock sbUpdated = sb;
        Ext2Utils::updateSuperblockFreeCounts(disk, sbUpdated);
        Ext2Utils::writeSuperblock(diskPath, sbUpdated, mbr, partitionIndex);
        
        disk.close();
        
        // ✅ NUEVO: Registrar en journal
        if (sb.s_filesystem_type == 3) {
            writeJournal(sb, partitionIndex, "chown", path, newUser);
        }
        
        // 14. Éxito
        result.success = true;
        result.message = "Propietario cambiado exitosamente: " + path + " → " + newUser + 
                         " (" + std::to_string(changedCount) + " elemento(s))";
        result.data["chown"] = {
            {"path", path},
            {"new_user", newUser},
            {"new_uid", newUid},
            {"new_gid", newGid},
            {"recursive", recursive},
            {"changed", changedCount}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar CHOWN: " + std::string(e.what());
    }
    
    return result;
}