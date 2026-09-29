#ifndef BLOCK_H
#define BLOCK_H

#include <cstdint>

#pragma pack(push, 1)

// Estructura para el contenido de un bloque de carpeta
struct FolderContent {
    char b_name[12];         // Nombre del archivo o carpeta
    int32_t b_inodo;         // Apuntador al inodo
};

// Bloque de carpeta (4 entradas de 16 bytes = 64 bytes)
struct BlockFolder {
    FolderContent b_content[4];
};

// Bloque de archivo (64 bytes de contenido)
struct BlockFile {
    char b_content[64];
};

// Bloque de apuntadores (16 apuntadores de 4 bytes = 64 bytes)
struct BlockPointer {
    int32_t b_pointer[16];
};

#pragma pack(pop)

#endif