#ifndef EXT2_UTILS_H
#define EXT2_UTILS_H

#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include "../structures/mbr.h"
#include "../structures/superblock.h"
#include "../structures/inode.h"
#include "../structures/block.h"

// ✅ sizeof reales con #pragma pack(1) en Linux x86_64:
//   sizeof(Inode)        = 104
//   sizeof(BlockFolder)  = 64
//   sizeof(BlockFile)    = 64
//   sizeof(BlockPointer) = 64
//   sizeof(Superblock)   = 84
#define INODE_SIZE   ((int)sizeof(Inode))
#define BLOCK_SIZE   ((int)sizeof(BlockFile))
#define NUM_DIRECT_BLOCKS 12
#define NUM_POINTERS 16

class Ext2Utils {
public:
    // ===== SUPERBLOCK =====
    static Superblock readSuperblock(const std::string& diskPath, const MBR& mbr, int partitionIndex);
    static bool writeSuperblock(const std::string& diskPath, const Superblock& sb, const MBR& mbr, int partitionIndex);
    
    // ===== BITMAPS =====
    static int findFreeInode(std::fstream& disk, const Superblock& sb);
    static int findFreeBlock(std::fstream& disk, const Superblock& sb);
    static void markInodeUsed(std::fstream& disk, const Superblock& sb, int inodeIndex);
    static void markInodeFree(std::fstream& disk, const Superblock& sb, int inodeIndex);
    static void markBlockUsed(std::fstream& disk, const Superblock& sb, int blockIndex);
    static void markBlockFree(std::fstream& disk, const Superblock& sb, int blockIndex);
    
    // ===== INODOS =====
    static Inode readInode(std::fstream& disk, const Superblock& sb, int inodeIndex);
    static bool writeInode(std::fstream& disk, const Superblock& sb, int inodeIndex, const Inode& inode);
    
    // ===== BLOQUES =====
    static BlockFolder readBlockFolder(std::fstream& disk, const Superblock& sb, int blockIndex);
    static bool writeBlockFolder(std::fstream& disk, const Superblock& sb, int blockIndex, const BlockFolder& block);
    static BlockFile readBlockFile(std::fstream& disk, const Superblock& sb, int blockIndex);
    static bool writeBlockFile(std::fstream& disk, const Superblock& sb, int blockIndex, const BlockFile& block);
    static BlockPointer readBlockPointer(std::fstream& disk, const Superblock& sb, int blockIndex);
    static bool writeBlockPointer(std::fstream& disk, const Superblock& sb, int blockIndex, const BlockPointer& block);
    
    // ===== PATH UTILITIES =====
    static int getRootInode();
    static std::vector<std::string> splitPath(const std::string& path);
    
    static int findInodeByPath(const std::string& diskPath, const std::string& path,
                               const Superblock& sb, const MBR& mbr, int partitionIndex);
    
    static int findInodeByPathInternal(std::fstream& disk, const std::string& path,
                                       const Superblock& sb);
    
    // ===== ARCHIVOS =====
    static std::string readFile(const std::string& diskPath, const std::string& filePath, 
                                const Superblock& sb, const MBR& mbr, int partitionIndex);
    static bool writeFile(const std::string& diskPath, const std::string& filePath, 
                          const std::string& content, const Superblock& sb, 
                          const MBR& mbr, int partitionIndex, int uid, int gid);
    
    // ===== CARPETAS =====
    static bool createDirectory(const std::string& diskPath, const std::string& dirPath,
                                const Superblock& sb, const MBR& mbr, 
                                int partitionIndex, int uid, int gid);
    
    // ===== USERS.TXT =====
    static std::vector<std::string> readUsersFile(const std::string& diskPath, 
                                                  const std::string& mountId,
                                                  const MBR& mbr, int partitionIndex);
    static bool writeUsersFile(const std::string& diskPath, 
                               const std::vector<std::string>& lines,
                               const MBR& mbr, int partitionIndex);
    
    // ===== UTILIDADES =====
    static void updateSuperblockFreeCounts(std::fstream& disk, Superblock& sb);
};

#endif