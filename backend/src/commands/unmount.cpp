#include "command_handler.h"
#include <fstream>
#include <cstring>

// ============================================================
// UNMOUNT: Desmonta una partición del sistema
// 
// Parámetros:
//   -id : Obligatorio. ID de la partición a desmontar.
//
// Comportamiento:
//   1. Busca el ID en mountedDisks
//   2. Si no existe, error
//   3. Lee el MBR del disco asociado
//   4. Busca la partición con ese part_id
//   5. Cambia part_status = '0'
//   6. Cambia part_correlative = 0
//   7. Limpia part_id
//   8. Escribe el MBR actualizado
//   9. Elimina el ID de mountedDisks
// ============================================================
CommandResult CommandHandler::processUnmount(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        // 1. Validar parámetro obligatorio
        if (!params.contains("id")) {
            result.message = "Error: El parámetro -id es obligatorio";
            return result;
        }
        
        std::string id = params["id"];
        
        // 2. Validar que el ID exista en memoria
        auto it = mountedDisks.find(id);
        if (it == mountedDisks.end()) {
            result.message = "Error: No existe una partición montada con el ID: " + id;
            return result;
        }
        
        // 3. Obtener la ruta del disco
        std::string diskPath = it->second;
        
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
        
        if (!disk.good()) {
            disk.close();
            result.message = "Error: No se pudo leer el MBR del disco";
            return result;
        }
        
        // 6. Buscar la partición con ese ID en el MBR
        int partitionIndex = -1;
        for (int i = 0; i < 4; i++) {
            // Comparar part_id con el ID buscado
            char partId[5] = {0};
            memcpy(partId, mbr.mbr_partitions[i].part_id, 4);
            partId[4] = '\0';
            std::string partIdStr(partId);
            
            if (partIdStr == id && mbr.mbr_partitions[i].part_s > 0) {
                partitionIndex = i;
                break;
            }
        }
        
        if (partitionIndex == -1) {
            disk.close();
            result.message = "Error: No se encontró la partición con el ID: " + id + " en el disco";
            return result;
        }
        
        // 7. Guardar info de la partición para el mensaje
        std::string partName(mbr.mbr_partitions[partitionIndex].part_name);
        partName = partName.c_str();
        
        // 8. Cambiar el estado de la partición
        mbr.mbr_partitions[partitionIndex].part_status = '0';
        mbr.mbr_partitions[partitionIndex].part_correlative = 0;
        memset(mbr.mbr_partitions[partitionIndex].part_id, 0, 4);
        
        // 9. Escribir el MBR actualizado
        disk.seekp(0, std::ios::beg);
        disk.write(reinterpret_cast<const char*>(&mbr), sizeof(MBR));
        disk.close();
        
        // 10. Eliminar de mountedDisks
        mountedDisks.erase(it);
        
        // 11. Éxito
        result.success = true;
        result.message = "Partición desmontada exitosamente. ID: " + id + " (Partición: " + partName + ")";
        result.data["unmount"] = {
            {"id", id},
            {"name", partName},
            {"disk", diskPath},
            {"partition", partitionIndex}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar UNMOUNT: " + std::string(e.what());
    }
    
    return result;
}