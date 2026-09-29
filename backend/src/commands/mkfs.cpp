#include "command_handler.h"
#include "../utils/ext2_utils.h"
#include <fstream>
#include <cstring>
#include <cmath>
#include <ctime>

#define EXT2_MAGIC 0xEF53
#define JOURNALING_CONSTANT 50

static void calculateStructuresEXT2(int64_t partitionSize, int& numInodes, int& numBlocks) {
    const int64_t SUPERBLOCK_SIZE = (int64_t)sizeof(Superblock);
    const int64_t INODE_REAL_SIZE = (int64_t)sizeof(Inode);
    const int64_t BLOCK_REAL_SIZE = (int64_t)sizeof(BlockFile);
    
    int64_t denominator = 4 + INODE_REAL_SIZE + 3 * BLOCK_REAL_SIZE;
    int64_t numerator = partitionSize - SUPERBLOCK_SIZE;
    
    if (numerator <= 0) { numInodes = 1; numBlocks = 3; return; }
    
    numInodes = (int)floor((double)numerator / denominator);
    numBlocks = numInodes * 3;
    
    if (numInodes < 1) numInodes = 1;
    if (numBlocks < 3) numBlocks = 3;
}

static void calculateStructuresEXT3(int64_t partitionSize, int& numInodes, int& numBlocks) {
    const int64_t SUPERBLOCK_SIZE = (int64_t)sizeof(Superblock);
    const int64_t JOURNALING_SIZE = JOURNALING_CONSTANT;
    const int64_t INODE_REAL_SIZE = (int64_t)sizeof(Inode);
    const int64_t BLOCK_REAL_SIZE = (int64_t)sizeof(BlockFile);
    
    int64_t denominator = JOURNALING_SIZE + 4 + INODE_REAL_SIZE + 3 * BLOCK_REAL_SIZE;
    int64_t numerator = partitionSize - SUPERBLOCK_SIZE;
    
    if (numerator <= 0) { numInodes = 1; numBlocks = 3; return; }
    
    numInodes = (int)floor((double)numerator / denominator);
    numBlocks = numInodes * 3;
    
    if (numInodes < 1) numInodes = 1;
    if (numBlocks < 3) numBlocks = 3;
}

CommandResult CommandHandler::processMkfs(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        std::string id = params["id"];
        std::string type = params.contains("type") ? std::string(params["type"]) : "full";
        std::string fs = params.contains("fs") ? std::string(params["fs"]) : "2fs";
        
        std::transform(type.begin(), type.end(), type.begin(), ::tolower);
        std::transform(fs.begin(), fs.end(), fs.begin(), ::tolower);
        
        if (type != "full") {
            result.message = "Error: Tipo de formateo inválido. Solo se permite 'full'";
            return result;
        }
        
        int fsNumber = 2;
        if (fs == "2fs") fsNumber = 2;
        else if (fs == "3fs") fsNumber = 3;
        else {
            result.message = "Error: Sistema de archivos inválido. Use '2fs' o '3fs'";
            return result;
        }
        
        if (mountedDisks.find(id) == mountedDisks.end()) {
            result.message = "Error: El ID de montaje no existe: " + id;
            return result;
        }
        
        std::string diskPath = mountedDisks[id];
        
        std::fstream disk(diskPath, std::ios::in | std::ios::out | std::ios::binary);
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
        
        if (mbr.mbr_partitions[partitionIndex].part_status != '1') {
            disk.close();
            result.message = "Error: La partición no está montada";
            return result;
        }
        
        Partition& partition = mbr.mbr_partitions[partitionIndex];
        int64_t partitionStart = partition.part_start;
        int64_t partitionSize = partition.part_s;
        
        int numInodes, numBlocks;
        if (fsNumber == 3) {
            calculateStructuresEXT3(partitionSize, numInodes, numBlocks);
        } else {
            calculateStructuresEXT2(partitionSize, numInodes, numBlocks);
        }
        
        Superblock sb;
        memset(&sb, 0, sizeof(Superblock));
        
        sb.s_filesystem_type = fsNumber;
        sb.s_inodes_count = numInodes;
        sb.s_blocks_count = numBlocks;
        sb.s_free_blocks_count = numBlocks;
        sb.s_free_inodes_count = numInodes;
        sb.s_mtime = time(nullptr);
        sb.s_umtime = time(nullptr);
        sb.s_mnt_count = 1;
        sb.s_magic = EXT2_MAGIC;
        sb.s_inode_s = (int)sizeof(Inode);
        sb.s_block_s = (int)sizeof(BlockFile);
        sb.s_first_ino = 0;
        sb.s_first_blo = 0;
        
        if (fsNumber == 3) {
            int64_t currentOffset = partitionStart + sizeof(Superblock);
            int journalingSize = JOURNALING_CONSTANT * numInodes;
            currentOffset += journalingSize;
            
            sb.s_bm_inode_start = currentOffset;
            int bmInodeSize = (numInodes + 7) / 8;
            currentOffset += bmInodeSize;
            
            sb.s_bm_block_start = currentOffset;
            int bmBlockSize = (numBlocks + 7) / 8;
            currentOffset += bmBlockSize;
            
            sb.s_inode_start = currentOffset;
            int inodeTableSize = numInodes * sizeof(Inode);
            currentOffset += inodeTableSize;
            
            sb.s_block_start = currentOffset;
            int blockTableSize = numBlocks * sizeof(BlockFile);
            currentOffset += blockTableSize;
        } else {
            int64_t currentOffset = partitionStart + sizeof(Superblock);
            
            sb.s_bm_inode_start = currentOffset;
            int bmInodeSize = (numInodes + 7) / 8;
            currentOffset += bmInodeSize;
            
            sb.s_bm_block_start = currentOffset;
            int bmBlockSize = (numBlocks + 7) / 8;
            currentOffset += bmBlockSize;
            
            sb.s_inode_start = currentOffset;
            int inodeTableSize = numInodes * sizeof(Inode);
            currentOffset += inodeTableSize;
            
            sb.s_block_start = currentOffset;
            int blockTableSize = numBlocks * sizeof(BlockFile);
            currentOffset += blockTableSize;
        }
        
        disk.seekp(partitionStart, std::ios::beg);
        disk.write(reinterpret_cast<const char*>(&sb), sizeof(Superblock));
        
        if (fsNumber == 3) {
            int64_t journalingStart = partitionStart + sizeof(Superblock);
            int64_t journalingSize = JOURNALING_CONSTANT * numInodes;
            
            char zeroBuf[1024] = {0};
            disk.seekp(journalingStart, std::ios::beg);
            int64_t written = 0;
            while (written < journalingSize) {
                int64_t chunk = std::min((int64_t)1024, journalingSize - written);
                disk.write(zeroBuf, chunk);
                written += chunk;
            }
            
            Journal firstJournal;
            memset(&firstJournal, 0, sizeof(Journal));
            firstJournal.j_count = 0;
            disk.seekp(journalingStart, std::ios::beg);
            disk.write(reinterpret_cast<const char*>(&firstJournal), sizeof(Journal));
        }
        
        int bmInodeSize = (numInodes + 7) / 8;
        int bmBlockSize = (numBlocks + 7) / 8;
        
        char zeroByte = 0;
        disk.seekp(sb.s_bm_inode_start, std::ios::beg);
        for (int i = 0; i < bmInodeSize; i++) disk.write(&zeroByte, 1);
        
        disk.seekp(sb.s_bm_block_start, std::ios::beg);
        for (int i = 0; i < bmBlockSize; i++) disk.write(&zeroByte, 1);
        
        disk.seekp(sb.s_inode_start, std::ios::beg);
        char zeroInode[sizeof(Inode)] = {0};
        for (int i = 0; i < numInodes; i++) disk.write(zeroInode, sizeof(Inode));
        
        disk.seekp(sb.s_block_start, std::ios::beg);
        char zeroBlock[sizeof(BlockFile)] = {0};
        for (int i = 0; i < numBlocks; i++) disk.write(zeroBlock, sizeof(BlockFile));
        
        Inode rootInode;
        memset(&rootInode, 0, sizeof(Inode));
        rootInode.i_uid = 1;
        rootInode.i_gid = 1;
        rootInode.i_s = 0;
        rootInode.i_atime = time(nullptr);
        rootInode.i_ctime = time(nullptr);
        rootInode.i_mtime = time(nullptr);
        for (int i = 0; i < 16; i++) rootInode.i_block[i] = -1;
        rootInode.i_type = '0';
        rootInode.i_perm[0] = '7';
        rootInode.i_perm[1] = '7';
        rootInode.i_perm[2] = '5';
        
        int rootBlockIndex = Ext2Utils::findFreeBlock(disk, sb);
        if (rootBlockIndex == -1) {
            disk.close();
            result.message = "Error: No hay bloques libres para la carpeta raíz";
            return result;
        }
        
        rootInode.i_block[0] = rootBlockIndex;
        Ext2Utils::markBlockUsed(disk, sb, rootBlockIndex);
        
        BlockFolder rootBlock;
        memset(&rootBlock, 0, sizeof(BlockFolder));
        strcpy(rootBlock.b_content[0].b_name, ".");
        rootBlock.b_content[0].b_inodo = 0;
        strcpy(rootBlock.b_content[1].b_name, "..");
        rootBlock.b_content[1].b_inodo = 0;
        rootBlock.b_content[2].b_inodo = -1;
        rootBlock.b_content[3].b_inodo = -1;
        
        Ext2Utils::writeBlockFolder(disk, sb, rootBlockIndex, rootBlock);
        
        Ext2Utils::writeInode(disk, sb, 0, rootInode);
        Ext2Utils::markInodeUsed(disk, sb, 0);
        
        sb.s_free_blocks_count = sb.s_blocks_count - 1;
        sb.s_free_inodes_count = sb.s_inodes_count - 1;
        
        disk.close();
        
        Ext2Utils::writeSuperblock(diskPath, sb, mbr, partitionIndex);
        
        // ============================================================
        // 18. Crear users.txt en la raíz
        // ============================================================
        std::string defaultUsers = "1, G, root\n1, U, root, root, 123\n";
        if (!Ext2Utils::writeFile(diskPath, "/users.txt", defaultUsers, sb, mbr, partitionIndex, 1, 1)) {
            result.message = "Error: No se pudo crear users.txt durante el formateo";
            return result;
        }
        
        Superblock sbFinal = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
        
        // ✅ FIX CRÍTICO: Cambiar permisos de /users.txt a 600
        int usersInode = Ext2Utils::findInodeByPath(diskPath, "/users.txt", sbFinal, mbr, partitionIndex);
        if (usersInode != -1) {
            std::fstream diskFix(diskPath, std::ios::in | std::ios::out | std::ios::binary);
            if (diskFix.is_open()) {
                Inode uInode = Ext2Utils::readInode(diskFix, sbFinal, usersInode);
                uInode.i_perm[0] = '6';  // Owner: rw-
                uInode.i_perm[1] = '0';  // Group: ---
                uInode.i_perm[2] = '0';  // Other: ---
                Ext2Utils::writeInode(diskFix, sbFinal, usersInode, uInode);
                diskFix.close();
            }
        }
        
        std::string fsName = (fsNumber == 3) ? "EXT3" : "EXT2";
        result.success = true;
        result.message = "Partición formateada exitosamente como " + fsName + " (Formateo completo)";
        result.data["format"] = {
            {"id", id},
            {"type", type},
            {"filesystem", fsName},
            {"fs_type", fsNumber},
            {"inodes", sbFinal.s_inodes_count},
            {"blocks", sbFinal.s_blocks_count},
            {"free_inodes", sbFinal.s_free_inodes_count},
            {"free_blocks", sbFinal.s_free_blocks_count}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar MKFS: " + std::string(e.what());
    }
    
    return result;
}