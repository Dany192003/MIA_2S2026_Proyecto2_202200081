#include "command_handler.h"
#include "../utils/ext2_utils.h"
#include <fstream>
#include <cstring>
#include <cmath>


#define EXT2_MAGIC 0xEF53

static void calculateStructures(int64_t partitionSize, int& numInodes, int& numBlocks) {
    const int64_t SUPERBLOCK_SIZE = (int64_t)sizeof(Superblock);  // 84
    const int64_t INODE_REAL_SIZE = (int64_t)sizeof(Inode);        // 104
    const int64_t BLOCK_REAL_SIZE = (int64_t)sizeof(BlockFile);    // 64
    
    int64_t denominator = 4 + INODE_REAL_SIZE + 3 * BLOCK_REAL_SIZE;
    int64_t numerator = partitionSize - SUPERBLOCK_SIZE;
    
    if (numerator <= 0) {
        numInodes = 1;
        numBlocks = 3;
        return;
    }
    
    numInodes = (int)floor((double)numerator / denominator);
    numBlocks = numInodes * 3;
    
    if (numInodes < 1) numInodes = 1;
    if (numBlocks < 3) numBlocks = 3;
}

CommandResult CommandHandler::processMkfs(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        // 1. Obtener parámetros
        std::string id = params["id"];
        std::string type = params.contains("type") ? std::string(params["type"]) : "full";
        
        std::transform(type.begin(), type.end(), type.begin(), ::tolower);
        
        if (type != "full") {
            result.message = "Error: Tipo de formateo inválido. Solo se permite 'full'";
            return result;
        }
        
        // 2. Validar que el ID exista en memoria
        if (mountedDisks.find(id) == mountedDisks.end()) {
            result.message = "Error: El ID de montaje no existe: " + id;
            return result;
        }
        
        // 3. Obtener la ruta del disco
        std::string diskPath = mountedDisks[id];
        
        // 4. Abrir el disco
        std::fstream disk(diskPath, std::ios::in | std::ios::out | std::ios::binary);
        if (!disk.is_open()) {
            result.message = "Error: No se pudo abrir el disco: " + diskPath;
            return result;
        }
        
        // 5. Leer el MBR
        MBR mbr;
        disk.seekg(0, std::ios::beg);
        disk.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        
        // 6. Buscar la partición por ID en el MBR
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
            result.message = "Error: No se encontró la partición para el ID: " + id;
            return result;
        }
        
        // 7. Verificar que la partición esté montada
        if (mbr.mbr_partitions[partitionIndex].part_status != '1') {
            result.message = "Error: La partición no está montada";
            return result;
        }
        
        // 8. Obtener información de la partición
        Partition& partition = mbr.mbr_partitions[partitionIndex];
        int64_t partitionStart = partition.part_start;
        int64_t partitionSize = partition.part_s;
        
        // 9. Calcular estructuras EXT2
        int numInodes, numBlocks;
        calculateStructures(partitionSize, numInodes, numBlocks);
        
        // 10. Crear Superbloque
        Superblock sb;
        memset(&sb, 0, sizeof(Superblock));
        
        sb.s_filesystem_type = 2;
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
        
        // Calcular offsets
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
        
        // 11. Escribir el Superbloque
        disk.seekp(partitionStart, std::ios::beg);
        disk.write(reinterpret_cast<const char*>(&sb), sizeof(Superblock));
        
        // 12. Inicializar Bitmaps
        char zeroByte = 0;
        disk.seekp(sb.s_bm_inode_start, std::ios::beg);
        for (int i = 0; i < bmInodeSize; i++) disk.write(&zeroByte, 1);
        
        disk.seekp(sb.s_bm_block_start, std::ios::beg);
        for (int i = 0; i < bmBlockSize; i++) disk.write(&zeroByte, 1);
        
        // 13. Inicializar tabla de inodos
        disk.seekp(sb.s_inode_start, std::ios::beg);
        char zeroInode[sizeof(Inode)] = {0};
        for (int i = 0; i < numInodes; i++) disk.write(zeroInode, sizeof(Inode));
        
        // 14. Inicializar tabla de bloques
        disk.seekp(sb.s_block_start, std::ios::beg);
        char zeroBlock[sizeof(BlockFile)] = {0};
        for (int i = 0; i < numBlocks; i++) disk.write(zeroBlock, sizeof(BlockFile));
        
        // ============================================================
        // 15. Crear el inodo raíz ("/") con bloque de carpeta
        // ============================================================
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
        
        // Asignar un bloque para la carpeta raíz
        int rootBlockIndex = Ext2Utils::findFreeBlock(disk, sb);
        if (rootBlockIndex == -1) {
            disk.close();
            result.message = "Error: No hay bloques libres para la carpeta raíz";
            return result;
        }
        
        rootInode.i_block[0] = rootBlockIndex;
        Ext2Utils::markBlockUsed(disk, sb, rootBlockIndex);
        
        // Inicializar bloque raíz con . y .. apuntando a sí mismo
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
        
        // Actualizar contadores de libres en el superbloque
        sb.s_free_blocks_count = sb.s_blocks_count - 1;
        sb.s_free_inodes_count = sb.s_inodes_count - 1;
        
        disk.close();
        
        // 16. Escribir el superbloque actualizado
        Ext2Utils::writeSuperblock(diskPath, sb, mbr, partitionIndex);
        
        // ============================================================
        // 17. Crear users.txt en la raíz
        // ============================================================
        std::string defaultUsers = "1, G, root\n1, U, root, root, 123\n";
        if (!Ext2Utils::writeFile(diskPath, "/users.txt", defaultUsers, sb, mbr, partitionIndex, 1, 1)) {
            result.message = "Error: No se pudo crear users.txt durante el formateo";
            return result;
        }
        
        // Recargar el superbloque actualizado (writeFile lo modificó)
        Superblock sbFinal = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
        
        result.success = true;
        result.message = "Partición formateada exitosamente como EXT2 (Formateo completo)";
        result.data["format"] = {
            {"id", id},
            {"type", type},
            {"filesystem", "ext2"},
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