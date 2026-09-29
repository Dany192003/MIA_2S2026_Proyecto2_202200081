#ifndef MBR_H
#define MBR_H

#include <ctime>
#include <cstdint>

#pragma pack(push, 1)

struct Partition {
    char part_status;          // '0' = inactiva, '1' = activa/montada
    char part_type;            // 'P' = Primaria, 'E' = Extendida
    char part_fit;             // 'B' = Best, 'F' = First, 'W' = Worst
    int32_t part_start;        // Byte donde inicia la partición
    int32_t part_s;            // Tamaño en bytes
    char part_name[16];        // Nombre de la partición
    int32_t part_correlative;  // -1 = no montada, 1..N
    char part_id[4];           // ID de montaje
};

struct MBR {
    int32_t mbr_tamano;              // Tamaño total del disco
    time_t mbr_fecha_creacion;       // Fecha de creación
    int32_t mbr_dsk_signature;       // Número random único
    char dsk_fit;                    // 'B', 'F', 'W'
    Partition mbr_partitions[4];     // 4 particiones
};

#pragma pack(pop)

#endif