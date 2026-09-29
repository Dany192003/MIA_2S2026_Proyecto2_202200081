#ifndef JOURNAL_H
#define JOURNAL_H

#include <cstdint>

#pragma pack(push, 1)

// ============================================================
// Information: contenido de una operación del journal
// ============================================================
struct Information {
    char i_operation[10];   // "mkdir", "mkfile", etc.
    char i_path[32];        // Ruta donde se realizó
    char i_content[64];     // Contenido (si es archivo)
    float i_date;           // Fecha (timestamp como float)
};

// ============================================================
// Journal: bitácora de operaciones
// ============================================================
struct Journal {
    int32_t j_count;        // Contador del journal
    Information j_content;  // Info de la acción
};

#pragma pack(pop)

#endif