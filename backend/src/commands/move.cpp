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
    if (uid == fileUid) writeBit = perms[0];
    else if (gid == fileGid) writeBit = perms[1];
    else writeBit = perms[2];
    
    int permValue = writeBit - '0';
    return (permValue & 2) != 0;
}

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
// Eliminar una entrada de una carpeta padre
// ============================================================
static bool removeEntryFromParent(std::fstream& disk, const Superblock& sb,
                                   int parentInodeIndex, const std::string& name,
                                   int expectedInode) {
    Inode parentInode = Ext2Utils::readInode(disk, sb, parentInodeIndex);
    
    for (int b = 0; b < 12; b++) {
        if (parentInode.i_block[b] == -1) continue;
        
        BlockFolder block = Ext2Utils::readBlockFolder(disk, sb, parentInode.i_block[b]);
        
        for (int i = 0; i < 4; i++) {
            std::string entryName(block.b_content[i].b_name);
            entryName = entryName.c_str();
            
            if (entryName == name && block.b_content[i].b_inodo == expectedInode) {
                memset(block.b_content[i].b_name, 0, 12);
                block.b_content[i].b_inodo = -1;
                Ext2Utils::writeBlockFolder(disk, sb, parentInode.i_block[b], block);
                return true;
            }
        }
    }
    
    return false;
}

// ============================================================
// Agregar una entrada a una carpeta padre
// ============================================================
static bool addEntryToParent(std::fstream& disk, const Superblock& sb,
                              int parentInodeIndex, const std::string& name,
                              int inode) {
    Inode parentInode = Ext2Utils::readInode(disk, sb, parentInodeIndex);
    
    for (int b = 0; b < 12; b++) {
        int blockIndex = parentInode.i_block[b];
        
        if (blockIndex == -1) {
            blockIndex = Ext2Utils::findFreeBlock(disk, sb);
            if (blockIndex == -1) return false;
            
            parentInode.i_block[b] = blockIndex;
            Ext2Utils::writeInode(disk, sb, parentInodeIndex, parentInode);
            Ext2Utils::markBlockUsed(disk, sb, blockIndex);
            
            BlockFolder emptyFolder;
            memset(&emptyFolder, 0, sizeof(BlockFolder));
            for (int i = 0; i < 4; i++) {
                emptyFolder.b_content[i].b_inodo = -1;
                memset(emptyFolder.b_content[i].b_name, 0, 12);
            }
            Ext2Utils::writeBlockFolder(disk, sb, blockIndex, emptyFolder);
        }
        
        BlockFolder block = Ext2Utils::readBlockFolder(disk, sb, blockIndex);
        
        for (int i = 0; i < 4; i++) {
            std::string entryName(block.b_content[i].b_name);
            entryName = entryName.c_str();
            
            if (entryName.empty() || block.b_content[i].b_inodo == -1) {
                strncpy(block.b_content[i].b_name, name.c_str(), 11);
                block.b_content[i].b_inodo = inode;
                Ext2Utils::writeBlockFolder(disk, sb, blockIndex, block);
                return true;
            }
        }
    }
    
    return false;
}

// ============================================================
// MOVE
// Según PDF: "Si el origen y destino están dentro de la misma partición,
// solo cambiará las referencias, para que ya no tenga el padre origen sino
// el padre destino"
// Solo se verifican permisos de ESCRITURA sobre el ORIGEN
// ============================================================
CommandResult CommandHandler::processMove(const json& params) {
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
        if (!params.contains("destino")) {
            result.message = "Error: El parámetro -destino es obligatorio";
            return result;
        }
        
        std::string path = params["path"];
        std::string destino = params["destino"];
        
        if (path.empty() || path == "/") {
            result.message = "Error: No se puede mover la raíz";
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
        
        // 8. Validar que el origen exista
        int srcInode = Ext2Utils::findInodeByPathInternal(disk, path, sb);
        if (srcInode == -1) {
            disk.close();
            result.message = "Error: No existe la ruta origen: " + path;
            return result;
        }
        
        // 9. Validar que el destino exista y sea carpeta
        int destInode = Ext2Utils::findInodeByPathInternal(disk, destino, sb);
        if (destInode == -1) {
            disk.close();
            result.message = "Error: No existe la ruta destino: " + destino;
            return result;
        }
        
        Inode destInodeStruct = Ext2Utils::readInode(disk, sb, destInode);
        if (destInodeStruct.i_type != '0') {
            disk.close();
            result.message = "Error: El destino no es una carpeta: " + destino;
            return result;
        }
        
        // 10. ✅ Según PDF: SOLO se verifican permisos de ESCRITURA sobre el ORIGEN
        Inode srcInodeStruct = Ext2Utils::readInode(disk, sb, srcInode);
        std::string srcPerms(srcInodeStruct.i_perm, 3);
        if (!hasWritePermission(srcPerms, user, uid, gid,
                                 srcInodeStruct.i_uid, srcInodeStruct.i_gid)) {
            disk.close();
            result.message = "Error: No tienes permisos de escritura sobre el origen: " + path;
            return result;
        }
        
        // 11. Calcular paths
        std::vector<std::string> srcParts = Ext2Utils::splitPath(path);
        std::string srcName = srcParts.back();
        srcParts.pop_back();
        
        std::string srcParentPath = "/";
        if (!srcParts.empty()) {
            srcParentPath += srcParts[0];
            for (size_t i = 1; i < srcParts.size(); i++) {
                srcParentPath += "/" + srcParts[i];
            }
        }
        
        int srcParentInode = Ext2Utils::findInodeByPathInternal(disk, srcParentPath, sb);
        if (srcParentInode == -1) {
            disk.close();
            result.message = "Error: No se encontró la carpeta padre del origen";
            return result;
        }
        
        // 12. Verificar que no se mueva a sí mismo dentro de su propio subárbol
        if (destino.rfind(path + "/", 0) == 0 || destino == path) {
            disk.close();
            result.message = "Error: No se puede mover una carpeta dentro de sí misma";
            return result;
        }
        
        // 13. Verificar que el destino final no exista ya
        std::string finalDestPath = destino;
        if (finalDestPath.back() != '/') finalDestPath += "/";
        finalDestPath += srcName;
        
        int existingDestInode = Ext2Utils::findInodeByPathInternal(disk, finalDestPath, sb);
        if (existingDestInode != -1) {
            disk.close();
            result.message = "Error: Ya existe un archivo o carpeta en el destino: " + finalDestPath;
            return result;
        }
        
        // 14. Eliminar entrada del padre origen
        if (!removeEntryFromParent(disk, sb, srcParentInode, srcName, srcInode)) {
            disk.close();
            result.message = "Error: No se pudo eliminar la entrada del padre origen";
            return result;
        }
        
        // 15. Agregar entrada al padre destino
        if (!addEntryToParent(disk, sb, destInode, srcName, srcInode)) {
            // Intentar restaurar la entrada original
            addEntryToParent(disk, sb, srcParentInode, srcName, srcInode);
            disk.close();
            result.message = "Error: No se pudo agregar la entrada al padre destino";
            return result;
        }
        
        // 16. Actualizar fecha de modificación del inodo
        srcInodeStruct.i_mtime = time(nullptr);
        Ext2Utils::writeInode(disk, sb, srcInode, srcInodeStruct);
        
        // 17. Actualizar superbloque
        Superblock sbUpdated = sb;
        Ext2Utils::updateSuperblockFreeCounts(disk, sbUpdated);
        Ext2Utils::writeSuperblock(diskPath, sbUpdated, mbr, partitionIndex);
        
        disk.close();
        
        // ✅ NUEVO: Registrar en journal
        if (sb.s_filesystem_type == 3) {
            writeJournal(sb, partitionIndex, "move", path, finalDestPath);
        }
        
        // 18. Éxito
        result.success = true;
        result.message = "Movido exitosamente: '" + path + "' → '" + finalDestPath + "'";
        result.data["move"] = {
            {"source", path},
            {"destino", finalDestPath},
            {"inode", srcInode}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar MOVE: " + std::string(e.what());
    }
    
    return result;
}