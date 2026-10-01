#include "command_handler.h"
#include "../utils/ext2_utils.h"
#include <fstream>
#include <cstring>
#include <ctime>
#include <vector>

// ============================================================
// Formatear timestamp (float) como string legible
// ============================================================
static std::string formatDate(float timestamp) {
    time_t t = (time_t)timestamp;
    struct tm* tm_info = localtime(&t);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M", tm_info);
    return std::string(buffer);
}

// ============================================================
// JOURNALING
// Según PDF:
// "Este mostrará la información de todas las transacciones realizadas
//  mostrando la operación, la ruta, contenido, fecha y hora."
// ============================================================
CommandResult CommandHandler::processJournaling(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        // 1. Validar parámetro
        if (!params.contains("id")) {
            result.message = "Error: El parámetro -id es obligatorio";
            return result;
        }
        
        std::string id = params["id"];
        
        // 2. Validar que el ID exista
        if (mountedDisks.find(id) == mountedDisks.end()) {
            result.message = "Error: El ID de montaje no existe: " + id;
            return result;
        }
        
        // 3. Obtener la ruta del disco
        std::string diskPath = mountedDisks[id];
        
        // 4. Abrir el disco
        std::fstream disk(diskPath, std::ios::in | std::ios::binary);
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
        
        // 9. Calcular ubicación del journal
        int64_t partitionStart = mbr.mbr_partitions[partitionIndex].part_start;
        int64_t journalStart = partitionStart + sizeof(Superblock);
        
        // 10. Leer el contador del Journal 0
        Journal firstJournal;
        disk.seekg(journalStart, std::ios::beg);
        disk.read(reinterpret_cast<char*>(&firstJournal), sizeof(Journal));
        
        int32_t totalEntries = firstJournal.j_count;
        if (totalEntries < 0) totalEntries = 0;
        if (totalEntries > 1000000) totalEntries = 0;
        
        // 11. Leer todas las entradas del journal
        json entries = json::array();
        int maxJournals = (50 * sb.s_inodes_count) / sizeof(Journal);
        if (maxJournals < 2) maxJournals = 2;
        if (maxJournals > 10000) maxJournals = 10000;
        
        for (int i = 1; i < maxJournals && (int)entries.size() < totalEntries; i++) {
            int64_t offset = journalStart + (int64_t)i * sizeof(Journal);
            
            // Verificar límites
            if (offset + sizeof(Journal) > partitionStart + mbr.mbr_partitions[partitionIndex].part_s) {
                break;
            }
            
            Journal j;
            disk.seekg(offset, std::ios::beg);
            disk.read(reinterpret_cast<char*>(&j), sizeof(Journal));
            
            // Si j_count == 0, no hay más entradas
            if (j.j_count == 0) break;
            
            // Extraer strings limpiando nulls
            std::string op(j.j_content.i_operation, 10);
            size_t nul = op.find('\0');
            if (nul != std::string::npos) op = op.substr(0, nul);
            
            std::string pathStr(j.j_content.i_path, 32);
            nul = pathStr.find('\0');
            if (nul != std::string::npos) pathStr = pathStr.substr(0, nul);
            
            std::string contentStr(j.j_content.i_content, 64);
            nul = contentStr.find('\0');
            if (nul != std::string::npos) contentStr = contentStr.substr(0, nul);
            
            json entry;
            entry["operation"] = op;
            entry["path"] = pathStr;
            entry["content"] = contentStr.empty() ? "—" : contentStr;
            entry["date"] = formatDate(j.j_content.i_date);
            entry["timestamp"] = j.j_content.i_date;
            entry["count"] = j.j_count;
            
            entries.push_back(entry);
        }
        
        disk.close();
        
        // 12. Éxito
        result.success = true;
        result.message = "Journaling leído exitosamente. " + std::to_string(entries.size()) + 
                         " transacción(es) encontrada(s).";
        result.data["journaling"] = {
            {"id", id},
            {"filesystem", "EXT3"},
            {"total_entries", (int)entries.size()},
            {"entries", entries}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar JOURNALING: " + std::string(e.what());
    }
    
    return result;
}