#include "command_handler.h"
#include "../utils/ext2_utils.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <vector>
#include <set>

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
// Obtener GID actual del usuario (reutilizado de cat.cpp)
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
// Eliminar un inodo y sus bloques
// ============================================================
static void freeInodeAndBlocks(std::fstream& disk, const Superblock& sb, int inodeIndex) {
    Inode inode = Ext2Utils::readInode(disk, sb, inodeIndex);
    
    // 1. Liberar bloques directos (0-11)
    for (int i = 0; i < 12; i++) {
        if (inode.i_block[i] != -1) {
            Ext2Utils::markBlockFree(disk, sb, inode.i_block[i]);
        }
    }
    
    // 2. Liberar simple indirecto (12)
    if (inode.i_block[12] != -1) {
        BlockPointer ptr = Ext2Utils::readBlockPointer(disk, sb, inode.i_block[12]);
        for (int j = 0; j < 16; j++) {
            if (ptr.b_pointer[j] == -1) break;
            Ext2Utils::markBlockFree(disk, sb, ptr.b_pointer[j]);
        }
        Ext2Utils::markBlockFree(disk, sb, inode.i_block[12]);
    }
    
    // 3. Liberar doble indirecto (13)
    if (inode.i_block[13] != -1) {
        BlockPointer ptr1 = Ext2Utils::readBlockPointer(disk, sb, inode.i_block[13]);
        for (int j = 0; j < 16; j++) {
            if (ptr1.b_pointer[j] == -1) break;
            BlockPointer ptr2 = Ext2Utils::readBlockPointer(disk, sb, ptr1.b_pointer[j]);
            for (int k = 0; k < 16; k++) {
                if (ptr2.b_pointer[k] == -1) break;
                Ext2Utils::markBlockFree(disk, sb, ptr2.b_pointer[k]);
            }
            Ext2Utils::markBlockFree(disk, sb, ptr1.b_pointer[j]);
        }
        Ext2Utils::markBlockFree(disk, sb, inode.i_block[13]);
    }
    
    // 4. Liberar triple indirecto (14)
    if (inode.i_block[14] != -1) {
        BlockPointer ptr1 = Ext2Utils::readBlockPointer(disk, sb, inode.i_block[14]);
        for (int j = 0; j < 16; j++) {
            if (ptr1.b_pointer[j] == -1) break;
            BlockPointer ptr2 = Ext2Utils::readBlockPointer(disk, sb, ptr1.b_pointer[j]);
            for (int k = 0; k < 16; k++) {
                if (ptr2.b_pointer[k] == -1) break;
                BlockPointer ptr3 = Ext2Utils::readBlockPointer(disk, sb, ptr2.b_pointer[k]);
                for (int l = 0; l < 16; l++) {
                    if (ptr3.b_pointer[l] == -1) break;
                    Ext2Utils::markBlockFree(disk, sb, ptr3.b_pointer[l]);
                }
                Ext2Utils::markBlockFree(disk, sb, ptr2.b_pointer[k]);
            }
            Ext2Utils::markBlockFree(disk, sb, ptr1.b_pointer[j]);
        }
        Ext2Utils::markBlockFree(disk, sb, inode.i_block[14]);
    }
    
    // 5. Marcar el inodo como libre
    Ext2Utils::markInodeFree(disk, sb, inodeIndex);
    
    // 6. Limpiar el inodo (memset)
    Inode emptyInode;
    memset(&emptyInode, 0, sizeof(Inode));
    for (int i = 0; i < 16; i++) emptyInode.i_block[i] = -1;
    Ext2Utils::writeInode(disk, sb, inodeIndex, emptyInode);
}

// ============================================================
// Recolectar todos los inodos a eliminar (recursivo)
// Retorna false si algún hijo no tiene permiso
// ============================================================
static bool collectInodesToDelete(std::fstream& disk, const Superblock& sb,
                                   int inodeIndex, int uid, int gid,
                                   const std::string& user,
                                   const std::vector<std::string>& userLines,
                                   std::vector<int>& inodesToDelete,
                                   std::vector<std::pair<int, std::string>>& parentEntriesToRemove) {
    Inode inode = Ext2Utils::readInode(disk, sb, inodeIndex);
    
    // Verificar permisos de escritura sobre este inodo
    std::string perms(inode.i_perm, 3);
    if (!hasWritePermission(perms, user, uid, gid, inode.i_uid, inode.i_gid)) {
        return false;
    }
    
    if (inode.i_type == '1') {
        // Es archivo → agregar a la lista
        inodesToDelete.push_back(inodeIndex);
        return true;
    }
    
    // Es carpeta → recorrer sus entradas
    for (int b = 0; b < 12; b++) {
        if (inode.i_block[b] == -1) continue;
        
        BlockFolder block = Ext2Utils::readBlockFolder(disk, sb, inode.i_block[b]);
        
        for (int i = 0; i < 4; i++) {
            std::string name(block.b_content[i].b_name);
            name = name.c_str();
            
            if (name.empty() || name == "." || name == "..") continue;
            if (block.b_content[i].b_inodo == -1) continue;
            
            int childInode = block.b_content[i].b_inodo;
            
            // Verificar permisos de escritura sobre el hijo
            Inode child = Ext2Utils::readInode(disk, sb, childInode);
            std::string childPerms(child.i_perm, 3);
            if (!hasWritePermission(childPerms, user, uid, gid, child.i_uid, child.i_gid)) {
                return false;  // No se puede eliminar el hijo → abortar todo
            }
            
            if (child.i_type == '0') {
                // Subcarpeta → recursión
                if (!collectInodesToDelete(disk, sb, childInode, uid, gid, user,
                                            userLines, inodesToDelete, parentEntriesToRemove)) {
                    return false;
                }
            } else {
                // Archivo → agregar
                inodesToDelete.push_back(childInode);
            }
        }
    }
    
    // Agregar la carpeta actual al final (post-order)
    inodesToDelete.push_back(inodeIndex);
    return true;
}

// ============================================================
// REMOVE
// ============================================================
CommandResult CommandHandler::processRemove(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        // 1. Validar sesión
        if (!isLoggedIn()) {
            result.message = "Error: No hay sesión activa. Use LOGIN primero.";
            return result;
        }
        
        // 2. Validar parámetro
        if (!params.contains("path")) {
            result.message = "Error: El parámetro -path es obligatorio";
            return result;
        }
        
        std::string path = params["path"];
        if (path.empty() || path == "/") {
            result.message = "Error: No se puede eliminar la raíz";
            return result;
        }
        
                // FIX CRÍTICO: users.txt solo lo puede borrar root
        if (path == "/users.txt" && !isRoot()) {
            result.message = "Error: Solo el usuario root puede eliminar /users.txt";
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
        
        // 7. Leer users.txt para obtener GID actual
        std::vector<std::string> userLines = Ext2Utils::readUsersFile(diskPath, mountId, mbr, partitionIndex);
        int currentGid = getCurrentUserGid(userLines, user);
        if (currentGid != 0) gid = currentGid;
        
        // 8. Buscar el inodo del path
        int targetInode = Ext2Utils::findInodeByPathInternal(disk, path, sb);
        if (targetInode == -1) {
            disk.close();
            result.message = "Error: No existe la ruta: " + path;
            return result;
        }
        
        // 9. Verificar permisos de escritura sobre el objetivo
        Inode targetInodeStruct = Ext2Utils::readInode(disk, sb, targetInode);
        std::string targetPerms(targetInodeStruct.i_perm, 3);
        if (!hasWritePermission(targetPerms, user, uid, gid, 
                                 targetInodeStruct.i_uid, targetInodeStruct.i_gid)) {
            disk.close();
            result.message = "Error: No tienes permisos de escritura sobre: " + path;
            return result;
        }
        
        // 10. Recolectar inodos a eliminar (recursivo)
        std::vector<int> inodesToDelete;
        std::vector<std::pair<int, std::string>> parentEntriesToRemove;
        
        bool canDelete = collectInodesToDelete(disk, sb, targetInode, uid, gid, user,
                                                userLines, inodesToDelete, parentEntriesToRemove);
        
        if (!canDelete) {
            disk.close();
            result.message = "Error: No se pudo eliminar '" + path + 
                             "' porque hay archivos o carpetas dentro sin permisos de escritura.";
            return result;
        }
        
        // 11. Eliminar los inodos y sus bloques
        for (int inodeIdx : inodesToDelete) {
            freeInodeAndBlocks(disk, sb, inodeIdx);
        }
        
        // 12. Eliminar la entrada del path en su carpeta padre
        std::vector<std::string> parts = Ext2Utils::splitPath(path);
        std::string fileName = parts.back();
        parts.pop_back();
        
        std::string parentPath = "/";
        if (!parts.empty()) {
            parentPath += parts[0];
            for (size_t i = 1; i < parts.size(); i++) {
                parentPath += "/" + parts[i];
            }
        }
        
        int parentInodeIndex = Ext2Utils::findInodeByPathInternal(disk, parentPath, sb);
        if (parentInodeIndex != -1) {
            Inode parentInode = Ext2Utils::readInode(disk, sb, parentInodeIndex);
            
            bool removed = false;
            for (int b = 0; b < 12 && !removed; b++) {
                if (parentInode.i_block[b] == -1) continue;
                
                BlockFolder block = Ext2Utils::readBlockFolder(disk, sb, parentInode.i_block[b]);
                for (int i = 0; i < 4; i++) {
                    std::string name(block.b_content[i].b_name);
                    name = name.c_str();
                    
                    if (name == fileName && block.b_content[i].b_inodo == targetInode) {
                        memset(block.b_content[i].b_name, 0, 12);
                        block.b_content[i].b_inodo = -1;
                        Ext2Utils::writeBlockFolder(disk, sb, parentInode.i_block[b], block);
                        removed = true;
                        break;
                    }
                }
            }
        }
        
        // 13. Actualizar superbloque
        Superblock sbUpdated = sb;
        Ext2Utils::updateSuperblockFreeCounts(disk, sbUpdated);
        Ext2Utils::writeSuperblock(diskPath, sbUpdated, mbr, partitionIndex);
        
        disk.close();
        
        // 14. Éxito
        result.success = true;
        result.message = "Eliminado exitosamente: " + path + 
                         " (" + std::to_string(inodesToDelete.size()) + " inodo(s))";
        result.data["remove"] = {
            {"path", path},
            {"inodes_deleted", (int)inodesToDelete.size()}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar REMOVE: " + std::string(e.what());
    }
    
    return result;
}