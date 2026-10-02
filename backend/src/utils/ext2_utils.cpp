#include "ext2_utils.h"
#include <sstream>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>      // readlink
#include <sys/stat.h>

namespace fs = std::filesystem;

// ============================================================
// RUTAS DE TRABAJO (✅ NUEVO)
// ============================================================

// Devuelve el directorio donde vive el ejecutable
// Ej: si el binario está en /home/edwin/proyecto/build/ext2fs
//     devuelve "/home/edwin/proyecto/build"
std::string Ext2Utils::getExecutableDir() {
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        std::string fullPath(buf);
        size_t lastSlash = fullPath.find_last_of('/');
        if (lastSlash != std::string::npos) {
            return fullPath.substr(0, lastSlash);
        }
    }
    // Fallback: directorio actual
    return ".";
}

// Devuelve la carpeta de discos y la crea si no existe
std::string Ext2Utils::getDiskDir() {
    // 1. Variable de entorno (prioridad)
    const char* env = std::getenv("EXT2_DISK_DIR");
    if (env && strlen(env) > 0) {
        std::string dir(env);
        if (dir.back() != '/') dir += "/";
        std::error_code ec;
        fs::create_directories(dir, ec);
        return dir;
    }
    
    // 2. Fallback: <ejecutable>/discos/
    std::string dir = getExecutableDir() + "/discos/";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

// Devuelve la carpeta de reportes y la crea si no existe
std::string Ext2Utils::getReportsDir() {
    // 1. Variable de entorno (prioridad)
    const char* env = std::getenv("EXT2_REPORTS_DIR");
    if (env && strlen(env) > 0) {
        std::string dir(env);
        if (dir.back() != '/') dir += "/";
        std::error_code ec;
        fs::create_directories(dir, ec);
        return dir;
    }
    
    // 2. Fallback: <ejecutable>/reports/
    std::string dir = getExecutableDir() + "/reports/";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

// ============================================================
// SUPERBLOCK
// ============================================================

Superblock Ext2Utils::readSuperblock(const std::string& diskPath, const MBR& mbr, int partitionIndex) {
    Superblock sb;
    memset(&sb, 0, sizeof(Superblock));
    
    std::fstream disk(diskPath, std::ios::in | std::ios::binary);
    if (!disk.is_open()) return sb;
    
    int64_t partitionStart = mbr.mbr_partitions[partitionIndex].part_start;
    disk.seekg(partitionStart, std::ios::beg);
    disk.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));
    disk.close();
    
    return sb;
}

bool Ext2Utils::writeSuperblock(const std::string& diskPath, const Superblock& sb, const MBR& mbr, int partitionIndex) {
    std::fstream disk(diskPath, std::ios::in | std::ios::out | std::ios::binary);
    if (!disk.is_open()) return false;
    
    int64_t partitionStart = mbr.mbr_partitions[partitionIndex].part_start;
    disk.seekp(partitionStart, std::ios::beg);
    disk.write(reinterpret_cast<const char*>(&sb), sizeof(Superblock));
    disk.close();
    
    return true;
}

// ============================================================
// BITMAPS
// ============================================================

int Ext2Utils::findFreeInode(std::fstream& disk, const Superblock& sb) {
    int bmSize = (sb.s_inodes_count + 7) / 8;
    disk.seekg(sb.s_bm_inode_start, std::ios::beg);
    
    for (int i = 0; i < bmSize; i++) {
        char byte;
        disk.read(&byte, 1);
        for (int bit = 0; bit < 8; bit++) {
            int index = i * 8 + bit;
            if (index >= sb.s_inodes_count) return -1;
            if (!(byte & (1 << bit))) {
                return index;
            }
        }
    }
    return -1;
}

int Ext2Utils::findFreeBlock(std::fstream& disk, const Superblock& sb) {
    int bmSize = (sb.s_blocks_count + 7) / 8;
    disk.seekg(sb.s_bm_block_start, std::ios::beg);
    
    for (int i = 0; i < bmSize; i++) {
        char byte;
        disk.read(&byte, 1);
        for (int bit = 0; bit < 8; bit++) {
            int index = i * 8 + bit;
            if (index >= sb.s_blocks_count) return -1;
            if (!(byte & (1 << bit))) {
                return index;
            }
        }
    }
    return -1;
}

void Ext2Utils::markInodeUsed(std::fstream& disk, const Superblock& sb, int inodeIndex) {
    int byteIndex = inodeIndex / 8;
    int bitIndex = inodeIndex % 8;
    
    disk.seekg(sb.s_bm_inode_start + byteIndex, std::ios::beg);
    char byte;
    disk.read(&byte, 1);
    byte |= (1 << bitIndex);
    disk.seekp(sb.s_bm_inode_start + byteIndex, std::ios::beg);
    disk.write(&byte, 1);
}

void Ext2Utils::markInodeFree(std::fstream& disk, const Superblock& sb, int inodeIndex) {
    int byteIndex = inodeIndex / 8;
    int bitIndex = inodeIndex % 8;
    
    disk.seekg(sb.s_bm_inode_start + byteIndex, std::ios::beg);
    char byte;
    disk.read(&byte, 1);
    byte &= ~(1 << bitIndex);
    disk.seekp(sb.s_bm_inode_start + byteIndex, std::ios::beg);
    disk.write(&byte, 1);
}

void Ext2Utils::markBlockUsed(std::fstream& disk, const Superblock& sb, int blockIndex) {
    int byteIndex = blockIndex / 8;
    int bitIndex = blockIndex % 8;
    
    disk.seekg(sb.s_bm_block_start + byteIndex, std::ios::beg);
    char byte;
    disk.read(&byte, 1);
    byte |= (1 << bitIndex);
    disk.seekp(sb.s_bm_block_start + byteIndex, std::ios::beg);
    disk.write(&byte, 1);
}

void Ext2Utils::markBlockFree(std::fstream& disk, const Superblock& sb, int blockIndex) {
    int byteIndex = blockIndex / 8;
    int bitIndex = blockIndex % 8;
    
    disk.seekg(sb.s_bm_block_start + byteIndex, std::ios::beg);
    char byte;
    disk.read(&byte, 1);
    byte &= ~(1 << bitIndex);
    disk.seekp(sb.s_bm_block_start + byteIndex, std::ios::beg);
    disk.write(&byte, 1);
}

// ============================================================
// INODOS
// ============================================================

Inode Ext2Utils::readInode(std::fstream& disk, const Superblock& sb, int inodeIndex) {
    Inode inode;
    memset(&inode, 0, sizeof(Inode));
    
    int64_t offset = sb.s_inode_start + (int64_t)inodeIndex * (int64_t)sizeof(Inode);
    disk.seekg(offset, std::ios::beg);
    disk.read(reinterpret_cast<char*>(&inode), sizeof(Inode));
    
    return inode;
}

bool Ext2Utils::writeInode(std::fstream& disk, const Superblock& sb, int inodeIndex, const Inode& inode) {
    int64_t offset = sb.s_inode_start + (int64_t)inodeIndex * (int64_t)sizeof(Inode);
    disk.seekp(offset, std::ios::beg);
    disk.write(reinterpret_cast<const char*>(&inode), sizeof(Inode));
    return true;
}

// ============================================================
// BLOQUES
// ============================================================

BlockFolder Ext2Utils::readBlockFolder(std::fstream& disk, const Superblock& sb, int blockIndex) {
    BlockFolder block;
    memset(&block, 0, sizeof(BlockFolder));
    
    int64_t offset = sb.s_block_start + (int64_t)blockIndex * (int64_t)sizeof(BlockFolder);
    disk.seekg(offset, std::ios::beg);
    disk.read(reinterpret_cast<char*>(&block), sizeof(BlockFolder));
    
    return block;
}

bool Ext2Utils::writeBlockFolder(std::fstream& disk, const Superblock& sb, int blockIndex, const BlockFolder& block) {
    int64_t offset = sb.s_block_start + (int64_t)blockIndex * (int64_t)sizeof(BlockFolder);
    disk.seekp(offset, std::ios::beg);
    disk.write(reinterpret_cast<const char*>(&block), sizeof(BlockFolder));
    return true;
}

BlockFile Ext2Utils::readBlockFile(std::fstream& disk, const Superblock& sb, int blockIndex) {
    BlockFile block;
    memset(&block, 0, sizeof(BlockFile));
    
    int64_t offset = sb.s_block_start + (int64_t)blockIndex * (int64_t)sizeof(BlockFile);
    disk.seekg(offset, std::ios::beg);
    disk.read(reinterpret_cast<char*>(&block), sizeof(BlockFile));
    
    return block;
}

bool Ext2Utils::writeBlockFile(std::fstream& disk, const Superblock& sb, int blockIndex, const BlockFile& block) {
    int64_t offset = sb.s_block_start + (int64_t)blockIndex * (int64_t)sizeof(BlockFile);
    disk.seekp(offset, std::ios::beg);
    disk.write(reinterpret_cast<const char*>(&block), sizeof(BlockFile));
    return true;
}

BlockPointer Ext2Utils::readBlockPointer(std::fstream& disk, const Superblock& sb, int blockIndex) {
    BlockPointer block;
    memset(&block, 0, sizeof(BlockPointer));
    
    int64_t offset = sb.s_block_start + (int64_t)blockIndex * (int64_t)sizeof(BlockPointer);
    disk.seekg(offset, std::ios::beg);
    disk.read(reinterpret_cast<char*>(&block), sizeof(BlockPointer));
    
    return block;
}

bool Ext2Utils::writeBlockPointer(std::fstream& disk, const Superblock& sb, int blockIndex, const BlockPointer& block) {
    int64_t offset = sb.s_block_start + (int64_t)blockIndex * (int64_t)sizeof(BlockPointer);
    disk.seekp(offset, std::ios::beg);
    disk.write(reinterpret_cast<const char*>(&block), sizeof(BlockPointer));
    return true;
}

// ============================================================
// PATH UTILITIES
// ============================================================

int Ext2Utils::getRootInode() {
    return 0;
}

std::vector<std::string> Ext2Utils::splitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    return parts;
}

// ============================================================
// findInodeByPathInternal recorre TODOS los bloques directos
// ============================================================
int Ext2Utils::findInodeByPathInternal(std::fstream& disk, const std::string& path,
                                       const Superblock& sb) {
    if (path == "/" || path.empty()) return 0;
    
    std::vector<std::string> parts = splitPath(path);
    if (parts.empty()) return -1;
    
    int currentInode = 0;
    
    for (const auto& part : parts) {
        Inode inode = readInode(disk, sb, currentInode);
        if (inode.i_type != '0') return -1;
        
        bool found = false;
        
        // Recorrer TODOS los bloques directos
        for (int b = 0; b < 12 && !found; b++) {
            if (inode.i_block[b] == -1) break;
            
            BlockFolder block = readBlockFolder(disk, sb, inode.i_block[b]);
            for (int i = 0; i < 4; i++) {
                std::string name(block.b_content[i].b_name);
                name = name.c_str();
                if (name == part) {
                    currentInode = block.b_content[i].b_inodo;
                    found = true;
                    break;
                }
            }
        }
        
        if (!found) return -1;
    }
    
    return currentInode;
}

int Ext2Utils::findInodeByPath(const std::string& diskPath, const std::string& path,
                               const Superblock& sb, const MBR& mbr, int partitionIndex) {
    (void)mbr;
    (void)partitionIndex;
    
    std::fstream disk(diskPath, std::ios::in | std::ios::binary);
    if (!disk.is_open()) return -1;
    
    int result = findInodeByPathInternal(disk, path, sb);
    disk.close();
    return result;
}

// ============================================================
// READ FILE
// ============================================================

std::string Ext2Utils::readFile(const std::string& diskPath, const std::string& filePath, 
                                const Superblock& sb, const MBR& mbr, int partitionIndex) {
    (void)mbr;
    (void)partitionIndex;
    
    std::fstream disk(diskPath, std::ios::in | std::ios::binary);
    if (!disk.is_open()) return "";
    
    int inodeIndex = findInodeByPathInternal(disk, filePath, sb);
    if (inodeIndex == -1) {
        disk.close();
        return "";
    }
    
    Inode inode = readInode(disk, sb, inodeIndex);
    if (inode.i_type != '1') {
        disk.close();
        return "";
    }
    
    std::string content;
    
    // Bloques directos (0-11)
    for (int i = 0; i < 12; i++) {
        if (inode.i_block[i] == -1) break;
        BlockFile block = readBlockFile(disk, sb, inode.i_block[i]);
        content.append(block.b_content, 64);
    }
    
    // Bloque simple indirecto (12)
    if (inode.i_block[12] != -1) {
        BlockPointer ptr = readBlockPointer(disk, sb, inode.i_block[12]);
        for (int j = 0; j < 16; j++) {
            if (ptr.b_pointer[j] == -1) break;
            BlockFile block = readBlockFile(disk, sb, ptr.b_pointer[j]);
            content.append(block.b_content, 64);
        }
    }
    
    // Bloque doble indirecto (13)
    if (inode.i_block[13] != -1) {
        BlockPointer ptr1 = readBlockPointer(disk, sb, inode.i_block[13]);
        for (int j = 0; j < 16; j++) {
            if (ptr1.b_pointer[j] == -1) break;
            BlockPointer ptr2 = readBlockPointer(disk, sb, ptr1.b_pointer[j]);
            for (int k = 0; k < 16; k++) {
                if (ptr2.b_pointer[k] == -1) break;
                BlockFile block = readBlockFile(disk, sb, ptr2.b_pointer[k]);
                content.append(block.b_content, 64);
            }
        }
    }
    
    // Bloque triple indirecto (14)
    if (inode.i_block[14] != -1) {
        BlockPointer ptr1 = readBlockPointer(disk, sb, inode.i_block[14]);
        for (int j = 0; j < 16; j++) {
            if (ptr1.b_pointer[j] == -1) break;
            BlockPointer ptr2 = readBlockPointer(disk, sb, ptr1.b_pointer[j]);
            for (int k = 0; k < 16; k++) {
                if (ptr2.b_pointer[k] == -1) break;
                BlockPointer ptr3 = readBlockPointer(disk, sb, ptr2.b_pointer[k]);
                for (int l = 0; l < 16; l++) {
                    if (ptr3.b_pointer[l] == -1) break;
                    BlockFile block = readBlockFile(disk, sb, ptr3.b_pointer[l]);
                    content.append(block.b_content, 64);
                }
            }
        }
    }
    
    disk.close();
    
    if (inode.i_s > 0 && (size_t)inode.i_s < content.length()) {
        content = content.substr(0, inode.i_s);
    }
    
    size_t lastNull = content.find_last_not_of('\0');
    if (lastNull != std::string::npos) {
        content = content.substr(0, lastNull + 1);
    }
    
    return content;
}

// ============================================================
// writeFile
// ============================================================

bool Ext2Utils::writeFile(const std::string& diskPath, const std::string& filePath, 
                          const std::string& content, const Superblock& sb, 
                          const MBR& mbr, int partitionIndex, int uid, int gid) {
    std::fstream disk(diskPath, std::ios::in | std::ios::out | std::ios::binary);
    if (!disk.is_open()) return false;
    
    std::vector<std::string> parts = splitPath(filePath);
    if (parts.empty()) {
        disk.close();
        return false;
    }
    
    std::string fileName = parts.back();
    parts.pop_back();
    
    if (fileName.length() > 11) {
        disk.close();
        return false;
    }
    
    std::string parentPath = "/";
    if (!parts.empty()) {
        parentPath += parts[0];
        for (size_t i = 1; i < parts.size(); i++) {
            parentPath += "/" + parts[i];
        }
    }
    
    int parentInodeIndex = findInodeByPathInternal(disk, parentPath, sb);
    if (parentInodeIndex == -1) {
        disk.close();
        return false;
    }
    
    Inode parentInode = readInode(disk, sb, parentInodeIndex);
    if (parentInode.i_type != '0') {
        disk.close();
        return false;
    }
    
    // Manejar sobrescritura
    int existingInode = findInodeByPathInternal(disk, filePath, sb);
    if (existingInode != -1) {
        for (int i = 0; i < 12; i++) {
            if (parentInode.i_block[i] != -1) {
                BlockFolder folderBlock = readBlockFolder(disk, sb, parentInode.i_block[i]);
                bool modified = false;
                for (int j = 0; j < 4; j++) {
                    std::string name(folderBlock.b_content[j].b_name);
                    name = name.c_str();
                    if (name == fileName) {
                        memset(folderBlock.b_content[j].b_name, 0, 12);
                        folderBlock.b_content[j].b_inodo = -1;
                        writeBlockFolder(disk, sb, parentInode.i_block[i], folderBlock);
                        modified = true;
                        break;
                    }
                }
                if (modified) break;
            }
        }
        
        Inode existing = readInode(disk, sb, existingInode);
        for (int i = 0; i < 16; i++) {
            if (existing.i_block[i] != -1) {
                markBlockFree(disk, sb, existing.i_block[i]);
            }
        }
        markInodeFree(disk, sb, existingInode);
    }
    
    // Crear nuevo inodo
    int newInodeIndex = findFreeInode(disk, sb);
    if (newInodeIndex == -1) {
        disk.close();
        return false;
    }
    
    size_t contentSize = content.length();
    int numBlocks = (contentSize + 63) / 64;
    if (numBlocks < 1) numBlocks = 1;
    
    int blockIndices[16];
    for (int i = 0; i < 16; i++) blockIndices[i] = -1;
    
    int remaining = numBlocks;
    int directBlocks = std::min(remaining, 12);
    remaining -= directBlocks;
    
    for (int i = 0; i < directBlocks; i++) {
        int blockIndex = findFreeBlock(disk, sb);
        if (blockIndex == -1) { disk.close(); return false; }
        blockIndices[i] = blockIndex;
        markBlockUsed(disk, sb, blockIndex);
    }
    
    // Simple indirecto
    std::vector<int> simpleDataBlocks;
    if (remaining > 0) {
        int ptrBlock = findFreeBlock(disk, sb);
        if (ptrBlock == -1) { disk.close(); return false; }
        blockIndices[12] = ptrBlock;
        markBlockUsed(disk, sb, ptrBlock);
        
        int simpleCount = std::min(remaining, 16);
        for (int i = 0; i < simpleCount; i++) {
            int dataBlock = findFreeBlock(disk, sb);
            if (dataBlock == -1) { disk.close(); return false; }
            simpleDataBlocks.push_back(dataBlock);
            markBlockUsed(disk, sb, dataBlock);
        }
        remaining -= simpleCount;
    }
    
    // Doble indirecto
    std::vector<int> doublePtr2Blocks;
    std::vector<int> doubleDataBlocks;
    if (remaining > 0) {
        int ptr1Block = findFreeBlock(disk, sb);
        if (ptr1Block == -1) { disk.close(); return false; }
        blockIndices[13] = ptr1Block;
        markBlockUsed(disk, sb, ptr1Block);
        
        while (remaining > 0) {
            int ptr2Block = findFreeBlock(disk, sb);
            if (ptr2Block == -1) { disk.close(); return false; }
            doublePtr2Blocks.push_back(ptr2Block);
            markBlockUsed(disk, sb, ptr2Block);
            
            int count = std::min(remaining, 16);
            for (int i = 0; i < count; i++) {
                int dataBlock = findFreeBlock(disk, sb);
                if (dataBlock == -1) { disk.close(); return false; }
                doubleDataBlocks.push_back(dataBlock);
                markBlockUsed(disk, sb, dataBlock);
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
        int ptr1Block = findFreeBlock(disk, sb);
        if (ptr1Block == -1) { disk.close(); return false; }
        blockIndices[14] = ptr1Block;
        markBlockUsed(disk, sb, ptr1Block);
        
        while (remaining > 0 && triplePtr2Blocks.size() < 16) {
            int ptr2Block = findFreeBlock(disk, sb);
            if (ptr2Block == -1) { disk.close(); return false; }
            triplePtr2Blocks.push_back(ptr2Block);
            markBlockUsed(disk, sb, ptr2Block);
            
            while (remaining > 0 && triplePtr3Blocks.size() < triplePtr2Blocks.size() * 16) {
                int ptr3Block = findFreeBlock(disk, sb);
                if (ptr3Block == -1) { disk.close(); return false; }
                triplePtr3Blocks.push_back(ptr3Block);
                markBlockUsed(disk, sb, ptr3Block);
                
                int count = std::min(remaining, 16);
                for (int i = 0; i < count; i++) {
                    int dataBlock = findFreeBlock(disk, sb);
                    if (dataBlock == -1) { disk.close(); return false; }
                    tripleDataBlocks.push_back(dataBlock);
                    markBlockUsed(disk, sb, dataBlock);
                }
                remaining -= count;
            }
        }
    }
    
    Inode newInode;
    memset(&newInode, 0, sizeof(Inode));
    newInode.i_uid = uid;
    newInode.i_gid = gid;
    newInode.i_s = contentSize;
    newInode.i_atime = time(nullptr);
    newInode.i_ctime = time(nullptr);
    newInode.i_mtime = time(nullptr);
    for (int i = 0; i < 16; i++) newInode.i_block[i] = blockIndices[i];
    newInode.i_type = '1';
    newInode.i_perm[0] = '6';
    newInode.i_perm[1] = '6';
    newInode.i_perm[2] = '4';
    
    writeInode(disk, sb, newInodeIndex, newInode);
    markInodeUsed(disk, sb, newInodeIndex);
    
    size_t offset = 0;
    for (int i = 0; i < directBlocks; i++) {
        BlockFile fileBlock;
        memset(&fileBlock, 0, sizeof(BlockFile));
        size_t toCopy = std::min((size_t)64, contentSize - offset);
        if (toCopy > 0) memcpy(fileBlock.b_content, content.c_str() + offset, toCopy);
        writeBlockFile(disk, sb, blockIndices[i], fileBlock);
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
            writeBlockFile(disk, sb, simpleDataBlocks[i], fileBlock);
            offset += 64;
        }
        writeBlockPointer(disk, sb, blockIndices[12], ptrBlock);
    }
    
    if (!doubleDataBlocks.empty()) {
        BlockPointer ptr1Block;
        memset(&ptr1Block, 0, sizeof(BlockPointer));
        for (int i = 0; i < 16; i++) ptr1Block.b_pointer[i] = -1;
        
        int dataIdx = 0;
        for (size_t i = 0; i < doublePtr2Blocks.size(); i++) {
            ptr1Block.b_pointer[i] = doublePtr2Blocks[i];
            BlockPointer ptr2Block;
            memset(&ptr2Block, 0, sizeof(BlockPointer));
            for (int j = 0; j < 16; j++) ptr2Block.b_pointer[j] = -1;
            
            for (int j = 0; j < 16 && dataIdx < (int)doubleDataBlocks.size(); j++) {
                ptr2Block.b_pointer[j] = doubleDataBlocks[dataIdx];
                BlockFile fileBlock;
                memset(&fileBlock, 0, sizeof(BlockFile));
                size_t toCopy = std::min((size_t)64, contentSize - offset);
                if (toCopy > 0) memcpy(fileBlock.b_content, content.c_str() + offset, toCopy);
                writeBlockFile(disk, sb, doubleDataBlocks[dataIdx], fileBlock);
                offset += 64;
                dataIdx++;
            }
            writeBlockPointer(disk, sb, doublePtr2Blocks[i], ptr2Block);
        }
        writeBlockPointer(disk, sb, blockIndices[13], ptr1Block);
    }
    
    if (!tripleDataBlocks.empty()) {
        BlockPointer ptr1Block;
        memset(&ptr1Block, 0, sizeof(BlockPointer));
        for (int i = 0; i < 16; i++) ptr1Block.b_pointer[i] = -1;
        
        int dataIdx = 0;
        int ptr3Idx = 0;
        for (size_t i = 0; i < triplePtr2Blocks.size(); i++) {
            ptr1Block.b_pointer[i] = triplePtr2Blocks[i];
            BlockPointer ptr2Block;
            memset(&ptr2Block, 0, sizeof(BlockPointer));
            for (int j = 0; j < 16; j++) ptr2Block.b_pointer[j] = -1;
            
            for (int j = 0; j < 16 && ptr3Idx < (int)triplePtr3Blocks.size(); j++) {
                ptr2Block.b_pointer[j] = triplePtr3Blocks[ptr3Idx];
                BlockPointer ptr3Block;
                memset(&ptr3Block, 0, sizeof(BlockPointer));
                for (int k = 0; k < 16; k++) ptr3Block.b_pointer[k] = -1;
                
                for (int k = 0; k < 16 && dataIdx < (int)tripleDataBlocks.size(); k++) {
                    ptr3Block.b_pointer[k] = tripleDataBlocks[dataIdx];
                    BlockFile fileBlock;
                    memset(&fileBlock, 0, sizeof(BlockFile));
                    size_t toCopy = std::min((size_t)64, contentSize - offset);
                    if (toCopy > 0) memcpy(fileBlock.b_content, content.c_str() + offset, toCopy);
                    writeBlockFile(disk, sb, tripleDataBlocks[dataIdx], fileBlock);
                    offset += 64;
                    dataIdx++;
                }
                writeBlockPointer(disk, sb, triplePtr3Blocks[ptr3Idx], ptr3Block);
                ptr3Idx++;
            }
            writeBlockPointer(disk, sb, triplePtr2Blocks[i], ptr2Block);
        }
        writeBlockPointer(disk, sb, blockIndices[14], ptr1Block);
    }
    
    Superblock sbUpdated = sb;
    sbUpdated.s_free_inodes_count--;
    int usedBlocks = directBlocks + (blockIndices[12] != -1 ? 1 : 0) + simpleDataBlocks.size()
                   + (blockIndices[13] != -1 ? 1 : 0) + doublePtr2Blocks.size() + doubleDataBlocks.size()
                   + (blockIndices[14] != -1 ? 1 : 0) + triplePtr2Blocks.size() + triplePtr3Blocks.size() + tripleDataBlocks.size();
    sbUpdated.s_free_blocks_count -= usedBlocks;
    if (sbUpdated.s_free_blocks_count < 0) sbUpdated.s_free_blocks_count = 0;
    if (sbUpdated.s_free_inodes_count < 0) sbUpdated.s_free_inodes_count = 0;
    
    // Buscar slot en TODOS los bloques directos del padre
    bool added = false;
    
    for (int b = 0; b < 12 && !added; b++) {
        int blockIndex = parentInode.i_block[b];
        
        if (blockIndex == -1) {
            blockIndex = findFreeBlock(disk, sb);
            if (blockIndex == -1) {
                disk.close();
                return false;
            }
            parentInode.i_block[b] = blockIndex;
            writeInode(disk, sb, parentInodeIndex, parentInode);
            markBlockUsed(disk, sb, blockIndex);
            
            BlockFolder folderBlock;
            memset(&folderBlock, 0, sizeof(BlockFolder));
            for (int i = 0; i < 4; i++) {
                folderBlock.b_content[i].b_inodo = -1;
                memset(folderBlock.b_content[i].b_name, 0, 12);
            }
            writeBlockFolder(disk, sb, blockIndex, folderBlock);
        }
        
        BlockFolder folderBlock = readBlockFolder(disk, sb, blockIndex);
        
        // ¿Ya existe?
        for (int i = 0; i < 4; i++) {
            std::string name(folderBlock.b_content[i].b_name);
            name = name.c_str();
            if (name == fileName && folderBlock.b_content[i].b_inodo != -1) {
                added = true;
                break;
            }
        }
        if (added) break;
        
        // Insertar en slot libre
        for (int i = 0; i < 4; i++) {
            std::string name(folderBlock.b_content[i].b_name);
            name = name.c_str();
            if (name.empty() || folderBlock.b_content[i].b_inodo == -1) {
                strncpy(folderBlock.b_content[i].b_name, fileName.c_str(), 11);
                folderBlock.b_content[i].b_inodo = newInodeIndex;
                writeBlockFolder(disk, sb, blockIndex, folderBlock);
                added = true;
                break;
            }
        }
    }
    
    if (!added) {
        disk.close();
        return false;
    }
    
    disk.seekp(mbr.mbr_partitions[partitionIndex].part_start, std::ios::beg);
    disk.write(reinterpret_cast<const char*>(&sbUpdated), sizeof(Superblock));
    
    disk.close();
    return true;
}

// ============================================================
// createDirectory
// ============================================================

bool Ext2Utils::createDirectory(const std::string& diskPath, const std::string& dirPath,
                                const Superblock& sb, const MBR& mbr, 
                                int partitionIndex, int uid, int gid) {
    std::fstream disk(diskPath, std::ios::in | std::ios::out | std::ios::binary);
    if (!disk.is_open()) return false;
    
    std::vector<std::string> parts = splitPath(dirPath);
    if (parts.empty()) {
        disk.close();
        return false;
    }
    
    std::string dirName = parts.back();
    parts.pop_back();
    
    if (dirName.length() > 11) {
        disk.close();
        return false;
    }
    
    std::string parentPath = "/";
    if (!parts.empty()) {
        parentPath += parts[0];
        for (size_t i = 1; i < parts.size(); i++) {
            parentPath += "/" + parts[i];
        }
    }
    
    int parentInodeIndex = findInodeByPathInternal(disk, parentPath, sb);
    if (parentInodeIndex == -1) {
        disk.close();
        return false;
    }
    
    Inode parentInode = readInode(disk, sb, parentInodeIndex);
    if (parentInode.i_type != '0') {
        disk.close();
        return false;
    }
    
    int existingInode = findInodeByPathInternal(disk, dirPath, sb);
    if (existingInode != -1) {
        disk.close();
        return false;
    }
    
    int newInodeIndex = findFreeInode(disk, sb);
    if (newInodeIndex == -1) {
        disk.close();
        return false;
    }
    
    int blockIndex = findFreeBlock(disk, sb);
    if (blockIndex == -1) {
        disk.close();
        return false;
    }
    markBlockUsed(disk, sb, blockIndex);
    
    BlockFolder folderBlock;
    memset(&folderBlock, 0, sizeof(BlockFolder));
    
    strcpy(folderBlock.b_content[0].b_name, ".");
    folderBlock.b_content[0].b_inodo = newInodeIndex;
    
    strcpy(folderBlock.b_content[1].b_name, "..");
    folderBlock.b_content[1].b_inodo = parentInodeIndex;
    
    folderBlock.b_content[2].b_inodo = -1;
    memset(folderBlock.b_content[2].b_name, 0, 12);
    folderBlock.b_content[3].b_inodo = -1;
    memset(folderBlock.b_content[3].b_name, 0, 12);
    
    writeBlockFolder(disk, sb, blockIndex, folderBlock);
    
    Inode newInode;
    memset(&newInode, 0, sizeof(Inode));
    newInode.i_uid = uid;
    newInode.i_gid = gid;
    newInode.i_s = 0;
    newInode.i_atime = time(nullptr);
    newInode.i_ctime = time(nullptr);
    newInode.i_mtime = time(nullptr);
    for (int i = 0; i < 16; i++) newInode.i_block[i] = -1;
    newInode.i_block[0] = blockIndex;
    newInode.i_type = '0';
    newInode.i_perm[0] = '6';
    newInode.i_perm[1] = '6';
    newInode.i_perm[2] = '4';
    
    writeInode(disk, sb, newInodeIndex, newInode);
    markInodeUsed(disk, sb, newInodeIndex);
    
    // Buscar slot en TODOS los bloques directos del padre
    bool added = false;
    
    for (int b = 0; b < 12 && !added; b++) {
        int parentBlockIndex = parentInode.i_block[b];
        
        if (parentBlockIndex == -1) {
            parentBlockIndex = findFreeBlock(disk, sb);
            if (parentBlockIndex == -1) {
                disk.close();
                return false;
            }
            parentInode.i_block[b] = parentBlockIndex;
            writeInode(disk, sb, parentInodeIndex, parentInode);
            markBlockUsed(disk, sb, parentBlockIndex);
            
            BlockFolder emptyFolder;
            memset(&emptyFolder, 0, sizeof(BlockFolder));
            for (int i = 0; i < 4; i++) {
                emptyFolder.b_content[i].b_inodo = -1;
                memset(emptyFolder.b_content[i].b_name, 0, 12);
            }
            writeBlockFolder(disk, sb, parentBlockIndex, emptyFolder);
        }
        
        BlockFolder parentFolder = readBlockFolder(disk, sb, parentBlockIndex);
        
        for (int i = 0; i < 4; i++) {
            std::string name(parentFolder.b_content[i].b_name);
            name = name.c_str();
            if (name.empty() || parentFolder.b_content[i].b_inodo == -1) {
                strncpy(parentFolder.b_content[i].b_name, dirName.c_str(), 11);
                parentFolder.b_content[i].b_inodo = newInodeIndex;
                writeBlockFolder(disk, sb, parentBlockIndex, parentFolder);
                added = true;
                break;
            }
        }
    }
    
    if (!added) {
        disk.close();
        return false;
    }
    
    Superblock sbUpdated = sb;
    sbUpdated.s_free_inodes_count--;
    sbUpdated.s_free_blocks_count--;
    if (sbUpdated.s_free_inodes_count < 0) sbUpdated.s_free_inodes_count = 0;
    if (sbUpdated.s_free_blocks_count < 0) sbUpdated.s_free_blocks_count = 0;
    
    disk.seekp(mbr.mbr_partitions[partitionIndex].part_start, std::ios::beg);
    disk.write(reinterpret_cast<const char*>(&sbUpdated), sizeof(Superblock));
    
    disk.close();
    return true;
}

// ============================================================
// USERS.TXT
// ============================================================

std::vector<std::string> Ext2Utils::readUsersFile(const std::string& diskPath, 
                                                  const std::string& mountId,
                                                  const MBR& mbr, int partitionIndex) {
    (void)mountId;
    
    std::vector<std::string> lines;
    Superblock sb = readSuperblock(diskPath, mbr, partitionIndex);
    
    int inodeIndex = findInodeByPath(diskPath, "/users.txt", sb, mbr, partitionIndex);
    
    if (inodeIndex != -1) {
        std::string content = readFile(diskPath, "/users.txt", sb, mbr, partitionIndex);
        std::stringstream ss(content);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty()) {
                lines.push_back(line);
            }
        }
    }
    
    if (lines.empty()) {
        lines.push_back("1, G, root");
        lines.push_back("1, U, root, root, 123");
    }
    
    return lines;
}

bool Ext2Utils::writeUsersFile(const std::string& diskPath, 
                               const std::vector<std::string>& lines,
                               const MBR& mbr, int partitionIndex) {
    std::string content;
    for (const auto& line : lines) {
        content += line + "\n";
    }
    
    Superblock sb = readSuperblock(diskPath, mbr, partitionIndex);
    return writeFile(diskPath, "/users.txt", content, sb, mbr, partitionIndex, 1, 1);
}

// ============================================================
// UTILIDADES
// ============================================================

void Ext2Utils::updateSuperblockFreeCounts(std::fstream& disk, Superblock& sb) {
    int freeInodes = 0;
    int bmInodeSize = (sb.s_inodes_count + 7) / 8;
    disk.seekg(sb.s_bm_inode_start, std::ios::beg);
    for (int i = 0; i < bmInodeSize; i++) {
        char byte;
        disk.read(&byte, 1);
        for (int bit = 0; bit < 8; bit++) {
            int idx = i * 8 + bit;
            if (idx >= sb.s_inodes_count) break;
            if (!(byte & (1 << bit))) freeInodes++;
        }
    }
    sb.s_free_inodes_count = freeInodes;
    
    int freeBlocks = 0;
    int bmBlockSize = (sb.s_blocks_count + 7) / 8;
    disk.seekg(sb.s_bm_block_start, std::ios::beg);
    for (int i = 0; i < bmBlockSize; i++) {
        char byte;
        disk.read(&byte, 1);
        for (int bit = 0; bit < 8; bit++) {
            int idx = i * 8 + bit;
            if (idx >= sb.s_blocks_count) break;
            if (!(byte & (1 << bit))) freeBlocks++;
        }
    }
    sb.s_free_blocks_count = freeBlocks;
}