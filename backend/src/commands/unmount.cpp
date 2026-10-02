#include "command_handler.h"
#include "../utils/ext2_utils.h"
#include <fstream>
#include <cstring>

// ============================================================
// UNMOUNT: Desmonta una partición del sistema
// 
// Comportamiento:
//   1. Busca el ID en mountedDisks
//   2. Si no existe, error
//   3. Lee el MBR del disco asociado
//   4. Busca la partición con ese part_id en MBR (primarias)
//   5. Si no está en MBR, busca en EBRs (lógicas)
//   6. Cambia part_status = '0', part_correlative = 0, limpia part_id
//   7. Escribe el MBR/EBR actualizado
//   8. Elimina el ID de mountedDisks
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
        
        // 6. Buscar la partición con ese ID en el MBR (primarias + extendida)
        int partitionIndex = -1;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_s <= 0) continue;
            
            char partId[5] = {0};
            memcpy(partId, mbr.mbr_partitions[i].part_id, 4);
            partId[4] = '\0';
            std::string partIdStr(partId);
            partIdStr = partIdStr.c_str();
            
            if (partIdStr == id) {
                partitionIndex = i;
                break;
            }
        }
        
        // 7. Si es primaria (está en MBR)
        if (partitionIndex != -1) {
            std::string partName(mbr.mbr_partitions[partitionIndex].part_name);
            partName = partName.c_str();
            
            // Limpiar estado
            mbr.mbr_partitions[partitionIndex].part_status = '0';
            mbr.mbr_partitions[partitionIndex].part_correlative = 0;
            memset(mbr.mbr_partitions[partitionIndex].part_id, 0, 4);
            
            // Escribir MBR actualizado
            disk.seekp(0, std::ios::beg);
            disk.write(reinterpret_cast<const char*>(&mbr), sizeof(MBR));
            disk.close();
            
            // Eliminar de mountedDisks
            mountedDisks.erase(it);
            
            result.success = true;
            result.message = "Partición desmontada exitosamente. ID: " + id + " (Partición: " + partName + ")";
            result.data["unmount"] = {
                {"id", id},
                {"name", partName},
                {"disk", diskPath},
                {"type", "P"},
                {"partition", partitionIndex}
            };
            return result;
        }
        
        // 8. No está en MBR → es una lógica
        // Como el EBR no tiene part_id, buscamos por part_mount='1'
        // y desmontamos la primera lógica montada
        int extendedSlot = -1;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_type == 'E' && mbr.mbr_partitions[i].part_s > 0) {
                extendedSlot = i;
                break;
            }
        }
        
        if (extendedSlot != -1) {
            int64_t currentPos = mbr.mbr_partitions[extendedSlot].part_start;
            
            while (true) {
                EBR ebr;
                disk.seekg(currentPos, std::ios::beg);
                disk.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
                
                if (!disk.good() || ebr.part_s == 0) break;
                
                if (ebr.part_mount == '1') {
                    // Desmontar esta lógica
                    std::string ebrName(ebr.part_name);
                    ebrName = ebrName.c_str();
                    
                    ebr.part_mount = '0';
                    
                    disk.seekp(currentPos, std::ios::beg);
                    disk.write(reinterpret_cast<const char*>(&ebr), sizeof(EBR));
                    disk.close();
                    
                    mountedDisks.erase(it);
                    
                    result.success = true;
                    result.message = "Partición desmontada exitosamente. ID: " + id + " (Partición: " + ebrName + ")";
                    result.data["unmount"] = {
                        {"id", id},
                        {"name", ebrName},
                        {"disk", diskPath},
                        {"type", "L"}
                    };
                    return result;
                }
                
                if (ebr.part_next == -1) break;
                currentPos = ebr.part_next;
            }
        }
        
        // 9. No se encontró (ni en MBR ni en EBR)
        disk.close();
        mountedDisks.erase(it);
        
        result.success = true;
        result.message = "Partición desmontada de memoria (ID: " + id + ")";
        result.data["unmount"] = {
            {"id", id},
            {"disk", diskPath},
            {"type", "?"}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar UNMOUNT: " + std::string(e.what());
    }
    
    return result;
}