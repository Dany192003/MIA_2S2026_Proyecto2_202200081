#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include <ctime>
#include <cstdint>

#pragma pack(push, 1)

struct Superblock {
    int32_t s_filesystem_type;   // 2 = EXT2
    int32_t s_inodes_count;      // Total de inodos
    int32_t s_blocks_count;      // Total de bloques
    int32_t s_free_blocks_count; // Bloques libres
    int32_t s_free_inodes_count; // Inodos libres
    time_t s_mtime;              // Último montaje
    time_t s_umtime;             // Último desmontaje
    int32_t s_mnt_count;         // Veces montado
    int32_t s_magic;             // 0xEF53
    int32_t s_inode_s;           // Tamaño del inodo
    int32_t s_block_s;           // Tamaño del bloque
    int32_t s_first_ino;         // Primer inodo libre
    int32_t s_first_blo;         // Primer bloque libre
    int32_t s_bm_inode_start;    // Inicio bitmap inodos
    int32_t s_bm_block_start;    // Inicio bitmap bloques
    int32_t s_inode_start;       // Inicio tabla inodos
    int32_t s_block_start;       // Inicio tabla bloques
};

#pragma pack(pop)

#endif