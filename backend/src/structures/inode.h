#ifndef INODE_H
#define INODE_H

#include <ctime>
#include <cstdint>

#pragma pack(push, 1)

struct Inode {
    int32_t i_uid;           // ID del usuario propietario
    int32_t i_gid;           // ID del grupo propietario
    int32_t i_s;             // Tamaño en bytes
    time_t i_atime;          // Último acceso
    time_t i_ctime;          // Fecha de creación
    time_t i_mtime;          // Última modificación
    int32_t i_block[16];     // 12 directos + 1 simple + 1 doble + 1 triple
    char i_type;             // '0' = Carpeta, '1' = Archivo
    char i_perm[3];          // Permisos UGO (octal)
};

#pragma pack(pop)

#endif