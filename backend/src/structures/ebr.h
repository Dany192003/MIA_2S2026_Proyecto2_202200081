#ifndef EBR_H
#define EBR_H

#include <cstdint>

#pragma pack(push, 1)

struct EBR {
    char part_mount;           // '0' = no montada, '1' = montada
    char part_fit;             // 'B', 'F', 'W'
    int32_t part_start;        // Inicio de la partición lógica
    int32_t part_s;            // Tamaño
    int32_t part_next;         // Byte del siguiente EBR, -1 si no hay
    char part_name[16];        // Nombre de la partición
};

#pragma pack(pop)

#endif