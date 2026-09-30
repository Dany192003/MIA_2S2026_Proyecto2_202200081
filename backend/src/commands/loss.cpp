#include "command_handler.h"
#include "../utils/ext2_utils.h"
#include <fstream>
#include <cstring>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

// ============================================================
// LOSS - Simular pérdida del sistema de archivos EXT3
// 
// Según PDF (página 27):
// "Este formatea los siguientes bloques de datos para simular un fallo en el disco
//  (una partición en específica), una inconsistencia o pérdida de información.
//  Se deberán limpiar los siguientes bloques con el carácter \0.
//    - Bloque de bitmap de Inodos
//    - Bloque de bitmap de Bloques
//    - Área de Inodos
//    - Área de Bloques."
//
// NO toca el superbloque (para que siga siendo un EXT3 válido)
// NO toca el journaling (para que el journaling siga funcionando)
// ============================================================

CommandResult CommandHandler::processLoss(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        // 1. Validar parámetro
        if (!params.contains("id")) {
            result.message = "Error: El parámetro -id es obligatorio";
            return result;
        }
        
        std::string id = params["id"];
        
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
        
        // 6. Buscar la partición por ID
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
        
        // 7. Leer el superbloque
        Superblock sb = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
        
        // 8. Validar que sea EXT3
        if (sb.s_filesystem_type != 3) {
            disk.close();
            result.message = "Error: La partición no es EXT3 (tipo actual: " + 
                             std::to_string(sb.s_filesystem_type) + "). Use mkfs -fs=3fs primero.";
            return result;
        }
        
        // 9. Validar magic
        if (sb.s_magic != 0xEF53) {
            disk.close();
            result.message = "Error: Superbloque inválido (magic incorrecto)";
            return result;
        }
        
        // ============================================================
        // 10. Guardar datos para los reportes ANTES
        // ============================================================
        std::string timestamp = std::to_string(time(nullptr));
        std::string reportsDir = "/home/Edwin/Desktop/MIA_2S2026_Proyecto2_202200081/reports/";
        
        // Generar reportes ANTES usando el sistema de reportes existente
        // (esto se hace desde el frontend con el comando rep, pero aquí guardamos estado)
        
        // ============================================================
        // 11. Limpiar los 4 bloques críticos con \0
        // ============================================================
        
        // Calcular tamaños
        int bmInodeSize = (sb.s_inodes_count + 7) / 8;
        int bmBlockSize = (sb.s_blocks_count + 7) / 8;
        int64_t inodeTableSize = (int64_t)sb.s_inodes_count * sizeof(Inode);
        int64_t blockTableSize = (int64_t)sb.s_blocks_count * sizeof(BlockFile);
        
        char zeroBuf[1024] = {0};
        int64_t totalZeroed = 0;
        
        // 11.1 Limpiar Bitmap de Inodos
        {
            disk.seekp(sb.s_bm_inode_start, std::ios::beg);
            int64_t written = 0;
            while (written < bmInodeSize) {
                int64_t chunk = std::min((int64_t)1024, (int64_t)bmInodeSize - written);
                disk.write(zeroBuf, chunk);
                written += chunk;
            }
            totalZeroed += bmInodeSize;
        }
        
        // 11.2 Limpiar Bitmap de Bloques
        {
            disk.seekp(sb.s_bm_block_start, std::ios::beg);
            int64_t written = 0;
            while (written < bmBlockSize) {
                int64_t chunk = std::min((int64_t)1024, (int64_t)bmBlockSize - written);
                disk.write(zeroBuf, chunk);
                written += chunk;
            }
            totalZeroed += bmBlockSize;
        }
        
        // 11.3 Limpiar Área de Inodos
        {
            disk.seekp(sb.s_inode_start, std::ios::beg);
            int64_t written = 0;
            while (written < inodeTableSize) {
                int64_t chunk = std::min((int64_t)1024, inodeTableSize - written);
                disk.write(zeroBuf, chunk);
                written += chunk;
            }
            totalZeroed += inodeTableSize;
        }
        
        // 11.4 Limpiar Área de Bloques
        {
            disk.seekp(sb.s_block_start, std::ios::beg);
            int64_t written = 0;
            while (written < blockTableSize) {
                int64_t chunk = std::min((int64_t)1024, blockTableSize - written);
                disk.write(zeroBuf, chunk);
                written += chunk;
            }
            totalZeroed += blockTableSize;
        }
        
        // 12. Cerrar el disco
        disk.close();
        
        // ============================================================
        // 13. Éxito
        // ============================================================
        result.success = true;
        result.message = "Pérdida del sistema de archivos simulada exitosamente en el ID: " + id;
        result.data["loss"] = {
            {"id", id},
            {"disk", diskPath},
            {"partition", partitionIndex},
            {"filesystem", "EXT3"},
            {"timestamp", timestamp},
            {"zeroed", {
                {"bitmap_inodes", bmInodeSize},
                {"bitmap_blocks", bmBlockSize},
                {"inode_table", inodeTableSize},
                {"block_table", blockTableSize},
                {"total_bytes", totalZeroed}
            }},
            {"note", "El superbloque y el journaling NO fueron modificados. Use 'rep -name=bm_inode' y 'rep -name=bm_block' para verificar."}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar LOSS: " + std::string(e.what());
    }
    
    return result;
}