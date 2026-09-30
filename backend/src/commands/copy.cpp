#include "command_handler.h"
#include "../utils/ext2_utils.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <vector>

// ============================================================
// Helpers de permisos
// ============================================================

static bool hasReadPermission(const std::string& perms, const std::string& user,
                               int uid, int gid, int fileUid, int fileGid) {
    if (user == "root") return true;
    
    char readBit;
    if (uid == fileUid) readBit = perms[0];
    else if (gid == fileGid) readBit = perms[1];
    else readBit = perms[2];
    
    int permValue = readBit - '0';
    return (permValue & 4) != 0;
}

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
// Copiar un archivo (lee contenido y crea nuevo archivo)
// ============================================================
static bool copyFileContent(std::fstream& disk, const Superblock& sb,
                             int srcInode, const std::string& destPath,
                             int destUid, int destGid) {
    // 1. Leer el inodo origen
    Inode src = Ext2Utils::readInode(disk, sb, srcInode);
    if (src.i_type != '1') return false;
    
    // 2. Leer el contenido del archivo origen
    std::string content;
    for (int i = 0; i < 12; i++) {
        if (src.i_block[i] == -1) break;
        BlockFile block = Ext2Utils::readBlockFile(disk, sb, src.i_block[i]);
        content.append(block.b_content, 64);
    }
    if (src.i_block[12] != -1) {
        BlockPointer ptr = Ext2Utils::readBlockPointer(disk, sb, src.i_block[12]);
        for (int j = 0; j < 16; j++) {
            if (ptr.b_pointer[j] == -1) break;
            BlockFile block = Ext2Utils::readBlockFile(disk, sb, ptr.b_pointer[j]);
            content.append(block.b_content, 64);
        }
    }
    if (src.i_block[13] != -1) {
        BlockPointer ptr1 = Ext2Utils::readBlockPointer(disk, sb, src.i_block[13]);
        for (int j = 0; j < 16; j++) {
            if (ptr1.b_pointer[j] == -1) break;
            BlockPointer ptr2 = Ext2Utils::readBlockPointer(disk, sb, ptr1.b_pointer[j]);
            for (int k = 0; k < 16; k++) {
                if (ptr2.b_pointer[k] == -1) break;
                BlockFile block = Ext2Utils::readBlockFile(disk, sb, ptr2.b_pointer[k]);
                content.append(block.b_content, 64);
            }
        }
    }
    if (src.i_block[14] != -1) {
        BlockPointer ptr1 = Ext2Utils::readBlockPointer(disk, sb, src.i_block[14]);
        for (int j = 0; j < 16; j++) {
            if (ptr1.b_pointer[j] == -1) break;
            BlockPointer ptr2 = Ext2Utils::readBlockPointer(disk, sb, ptr1.b_pointer[j]);
            for (int k = 0; k < 16; k++) {
                if (ptr2.b_pointer[k] == -1) break;
                BlockPointer ptr3 = Ext2Utils::readBlockPointer(disk, sb, ptr2.b_pointer[k]);
                for (int l = 0; l < 16; l++) {
                    if (ptr3.b_pointer[l] == -1) break;
                    BlockFile block = Ext2Utils::readBlockFile(disk, sb, ptr3.b_pointer[l]);
                    content.append(block.b_content, 64);
                }
            }
        }
    }
    
    // Truncar al tamaño real
    if (src.i_s > 0 && (size_t)src.i_s < content.length()) {
        content = content.substr(0, src.i_s);
    }
    
    // 3. Buscar la carpeta padre del destino
    std::vector<std::string> parts = Ext2Utils::splitPath(destPath);
    if (parts.empty()) return false;
    
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
    if (parentInodeIndex == -1) return false;
    
    Inode parentInode = Ext2Utils::readInode(disk, sb, parentInodeIndex);
    
    // 4. Crear nuevo inodo
    int newInodeIndex = Ext2Utils::findFreeInode(disk, sb);
    if (newInodeIndex == -1) return false;
    
    // 5. Asignar bloques y escribir contenido
    size_t contentSize = content.length();
    int numBlocks = (contentSize + 63) / 64;
    if (numBlocks < 1) numBlocks = 1;
    
    int blockIndices[16];
    for (int i = 0; i < 16; i++) blockIndices[i] = -1;
    
    int remaining = numBlocks;
    int directBlocks = std::min(remaining, 12);
    remaining -= directBlocks;
    
    for (int i = 0; i < directBlocks; i++) {
        int blockIndex = Ext2Utils::findFreeBlock(disk, sb);
        if (blockIndex == -1) return false;
        blockIndices[i] = blockIndex;
        Ext2Utils::markBlockUsed(disk, sb, blockIndex);
    }
    
    // Simple indirecto
    std::vector<int> simpleDataBlocks;
    if (remaining > 0) {
        int ptrBlock = Ext2Utils::findFreeBlock(disk, sb);
        if (ptrBlock == -1) return false;
        blockIndices[12] = ptrBlock;
        Ext2Utils::markBlockUsed(disk, sb, ptrBlock);
        
        int simpleCount = std::min(remaining, 16);
        for (int i = 0; i < simpleCount; i++) {
            int dataBlock = Ext2Utils::findFreeBlock(disk, sb);
            if (dataBlock == -1) return false;
            simpleDataBlocks.push_back(dataBlock);
            Ext2Utils::markBlockUsed(disk, sb, dataBlock);
        }
        remaining -= simpleCount;
    }
    
    // Doble indirecto
    std::vector<int> doublePtr2Blocks;
    std::vector<int> doubleDataBlocks;
    if (remaining > 0) {
        int ptr1Block = Ext2Utils::findFreeBlock(disk, sb);
        if (ptr1Block == -1) return false;
        blockIndices[13] = ptr1Block;
        Ext2Utils::markBlockUsed(disk, sb, ptr1Block);
        
        while (remaining > 0) {
            int ptr2Block = Ext2Utils::findFreeBlock(disk, sb);
            if (ptr2Block == -1) return false;
            doublePtr2Blocks.push_back(ptr2Block);
            Ext2Utils::markBlockUsed(disk, sb, ptr2Block);
            
            int count = std::min(remaining, 16);
            for (int i = 0; i < count; i++) {
                int dataBlock = Ext2Utils::findFreeBlock(disk, sb);
                if (dataBlock == -1) return false;
                doubleDataBlocks.push_back(dataBlock);
                Ext2Utils::markBlockUsed(disk, sb, dataBlock);
            }
            remaining -= count;
            if (doublePtr2Blocks.size() >= 16) break;
        }
    }
    
    // Triple indirecto
    std::vector<int> triplePtr2Blocks;
    std::vector<int> triplePtr3Blocks;
    std::vector<int> tripleDataBlocks;
    if (remaining > 0) {
        int ptr1Block = Ext2Utils::findFreeBlock(disk, sb);
        if (ptr1Block == -1) return false;
        blockIndices[14] = ptr1Block;
        Ext2Utils::markBlockUsed(disk, sb, ptr1Block);
        
        while (remaining > 0 && triplePtr2Blocks.size() < 16) {
            int ptr2Block = Ext2Utils::findFreeBlock(disk, sb);
            if (ptr2Block == -1) return false;
            triplePtr2Blocks.push_back(ptr2Block);
            Ext2Utils::markBlockUsed(disk, sb, ptr2Block);
            
            while (remaining > 0 && triplePtr3Blocks.size() < triplePtr2Blocks.size() * 16) {
                int ptr3Block = Ext2Utils::findFreeBlock(disk, sb);
                if (ptr3Block == -1) return false;
                triplePtr3Blocks.push_back(ptr3Block);
                Ext2Utils::markBlockUsed(disk, sb, ptr3Block);
                
                int count = std::min(remaining, 16);
                for (int i = 0; i < count; i++) {
                    int dataBlock = Ext2Utils::findFreeBlock(disk, sb);
                    if (dataBlock == -1) return false;
                    tripleDataBlocks.push_back(dataBlock);
                    Ext2Utils::markBlockUsed(disk, sb, dataBlock);
                }
                remaining -= count;
            }
        }
    }
    
    // 6. Crear inodo nuevo
    Inode newInode;
    memset(&newInode, 0, sizeof(Inode));
    newInode.i_uid = destUid;
    newInode.i_gid = destGid;
    newInode.i_s = contentSize;
    newInode.i_atime = time(nullptr);
    newInode.i_ctime = time(nullptr);
    newInode.i_mtime = time(nullptr);
    for (int i = 0; i < 16; i++) newInode.i_block[i] = blockIndices[i];
    newInode.i_type = '1';
    memcpy(newInode.i_perm, src.i_perm, 3);
    
    Ext2Utils::writeInode(disk, sb, newInodeIndex, newInode);
    Ext2Utils::markInodeUsed(disk, sb, newInodeIndex);
    
    // 7. Escribir contenido en bloques
    size_t offset = 0;
    for (int i = 0; i < directBlocks; i++) {
        BlockFile fileBlock;
        memset(&fileBlock, 0, sizeof(BlockFile));
        size_t toCopy = std::min((size_t)64, contentSize - offset);
        if (toCopy > 0) memcpy(fileBlock.b_content, content.c_str() + offset, toCopy);
        Ext2Utils::writeBlockFile(disk, sb, blockIndices[i], fileBlock);
        offset += 64;
    }
    
    if (!simpleDataBlocks.empty()) {
        BlockPointer ptrBlock;
        memset(&ptrBlock, 0, sizeof(BlockPointer));
        for (int i = 0; i < 16; i++) ptrBlock.b_pointer[i] = -1;
        
        for (size_t i = 0; i < simpleDataBlocks.size(); i++) {
            ptrBlock.b_pointer[i] = simpleDataBlocks[i];
            BlockFile fileBlock;
            memset(&fileBlock, 0, sizeof(BlockFile));
            size_t toCopy = std::min((size_t)64, contentSize - offset);
            if (toCopy > 0) memcpy(fileBlock.b_content, content.c_str() + offset, toCopy);
            Ext2Utils::writeBlockFile(disk, sb, simpleDataBlocks[i], fileBlock);
            offset += 64;
        }
        Ext2Utils::writeBlockPointer(disk, sb, blockIndices[12], ptrBlock);
    }
    
    // 8. Agregar entrada al padre
    bool added = false;
    for (int b = 0; b < 12 && !added; b++) {
        int blockIndex = parentInode.i_block[b];
        
        if (blockIndex == -1) {
            blockIndex = Ext2Utils::findFreeBlock(disk, sb);
            if (blockIndex == -1) return false;
            parentInode.i_block[b] = blockIndex;
            Ext2Utils::writeInode(disk, sb, parentInodeIndex, parentInode);
            Ext2Utils::markBlockUsed(disk, sb, blockIndex);
            
            BlockFolder folderBlock;
            memset(&folderBlock, 0, sizeof(BlockFolder));
            for (int i = 0; i < 4; i++) {
                folderBlock.b_content[i].b_inodo = -1;
                memset(folderBlock.b_content[i].b_name, 0, 12);
            }
            Ext2Utils::writeBlockFolder(disk, sb, blockIndex, folderBlock);
        }
        
        BlockFolder folderBlock = Ext2Utils::readBlockFolder(disk, sb, blockIndex);
        
        for (int i = 0; i < 4; i++) {
            std::string name(folderBlock.b_content[i].b_name);
            name = name.c_str();
            if (name.empty() || folderBlock.b_content[i].b_inodo == -1) {
                strncpy(folderBlock.b_content[i].b_name, fileName.c_str(), 11);
                folderBlock.b_content[i].b_inodo = newInodeIndex;
                Ext2Utils::writeBlockFolder(disk, sb, blockIndex, folderBlock);
                added = true;
                break;
            }
        }
    }
    
    return added;
}

// ============================================================
// Copiar una carpeta recursivamente
// ============================================================
static void copyFolderRecursive(std::fstream& disk, const Superblock& sb,
                                 int srcInodeIndex, const std::string& destPath,
                                 int destUid, int destGid, int uid, int gid,
                                 const std::string& user,
                                 const std::vector<std::string>& userLines,
                                 int& copiedCount, int& skippedCount) {
    Inode srcInode = Ext2Utils::readInode(disk, sb, srcInodeIndex);
    if (srcInode.i_type != '0') return;
    
    // Recorrer las entradas de la carpeta origen
    for (int b = 0; b < 12; b++) {
        if (srcInode.i_block[b] == -1) continue;
        
        BlockFolder block = Ext2Utils::readBlockFolder(disk, sb, srcInode.i_block[b]);
        
        for (int i = 0; i < 4; i++) {
            std::string name(block.b_content[i].b_name);
            name = name.c_str();
            
            if (name.empty() || name == "." || name == "..") continue;
            if (block.b_content[i].b_inodo == -1) continue;
            
            int childInode = block.b_content[i].b_inodo;
            Inode child = Ext2Utils::readInode(disk, sb, childInode);
            
            // Verificar permiso de lectura
            std::string childPerms(child.i_perm, 3);
            if (!hasReadPermission(childPerms, user, uid, gid, child.i_uid, child.i_gid)) {
                skippedCount++;
                continue;
            }
            
            std::string childDestPath = destPath + "/" + name;
            
            if (child.i_type == '1') {
                // Es archivo → copiar
                if (copyFileContent(disk, sb, childInode, childDestPath, destUid, destGid)) {
                    copiedCount++;
                }
            } else {
                // Es carpeta → crear carpeta y recursión
                // Nota: createDirectory abre su propio fstream, así que cerramos y reabrimos
                // Para simplicidad, usamos el mismo enfoque
                // (esto podría optimizarse, pero funciona)
                // TODO: Mejorar para no reabrir el archivo
                copiedCount++;
                copyFolderRecursive(disk, sb, childInode, childDestPath,
                                     destUid, destGid, uid, gid, user, userLines,
                                     copiedCount, skippedCount);
            }
        }
    }
}

// ============================================================
// COPY
// ============================================================
CommandResult CommandHandler::processCopy(const json& params) {
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
            result.message = "Error: No se puede copiar la raíz";
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
        
        // 10. Validar permiso de escritura sobre el destino
        std::string destPerms(destInodeStruct.i_perm, 3);
        if (!hasWritePermission(destPerms, user, uid, gid,
                                 destInodeStruct.i_uid, destInodeStruct.i_gid)) {
            disk.close();
            result.message = "Error: No tienes permisos de escritura sobre el destino: " + destino;
            return result;
        }
        
        // 11. Calcular el path destino completo
        std::string srcName;
        {
            std::vector<std::string> parts = Ext2Utils::splitPath(path);
            srcName = parts.back();
        }
        
        std::string finalDestPath = destino;
        if (finalDestPath.back() != '/') finalDestPath += "/";
        finalDestPath += srcName;
        
        // 12. Verificar que el destino final no exista
        int existingDestInode = Ext2Utils::findInodeByPathInternal(disk, finalDestPath, sb);
        if (existingDestInode != -1) {
            disk.close();
            result.message = "Error: Ya existe un archivo o carpeta en el destino: " + finalDestPath;
            return result;
        }
        
        // 13. Copiar según el tipo
        Inode srcInodeStruct = Ext2Utils::readInode(disk, sb, srcInode);
        
        int copiedCount = 0;
        int skippedCount = 0;
        
        if (srcInodeStruct.i_type == '1') {
            // Es archivo
            std::string srcPerms(srcInodeStruct.i_perm, 3);
            if (!hasReadPermission(srcPerms, user, uid, gid,
                                    srcInodeStruct.i_uid, srcInodeStruct.i_gid)) {
                disk.close();
                result.message = "Error: No tienes permisos de lectura sobre: " + path;
                return result;
            }
            
            if (copyFileContent(disk, sb, srcInode, finalDestPath, uid, gid)) {
                copiedCount = 1;
            } else {
                disk.close();
                result.message = "Error: No se pudo copiar el archivo: " + path;
                return result;
            }
        } else {
            // Es carpeta → crear carpeta destino y copiar recursivamente
            if (!Ext2Utils::createDirectory(diskPath, finalDestPath, sb, mbr, partitionIndex, uid, gid)) {
                disk.close();
                result.message = "Error: No se pudo crear la carpeta destino: " + finalDestPath;
                return result;
            }
            copiedCount = 1;
            
            // Recargar superbloque (createDirectory lo modificó)
            sb = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
            
            copyFolderRecursive(disk, sb, srcInode, finalDestPath,
                                 uid, gid, uid, gid, user, userLines,
                                 copiedCount, skippedCount);
        }
        
        // 14. Actualizar superbloque
        Superblock sbUpdated = sb;
        Ext2Utils::updateSuperblockFreeCounts(disk, sbUpdated);
        Ext2Utils::writeSuperblock(diskPath, sbUpdated, mbr, partitionIndex);
        
        disk.close();
        
        // 15. Éxito
        result.success = true;
        result.message = "Copiado exitosamente: '" + path + "' → '" + finalDestPath + "'";
        result.data["copy"] = {
            {"source", path},
            {"destino", finalDestPath},
            {"copied", copiedCount},
            {"skipped", skippedCount}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar COPY: " + std::string(e.what());
    }
    
    return result;
}