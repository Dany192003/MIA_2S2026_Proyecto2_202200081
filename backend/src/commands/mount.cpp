#include "command_handler.h"
#include "../utils/ext2_utils.h"
#include <fstream>
#include <cstring>
#include <set>

// ✅ Resuelve una ruta relativa contra EXT2_DISK_DIR
static std::string resolveDiskPath(const std::string& inputPath) {
    if (inputPath.empty()) return inputPath;
    if (inputPath[0] == '/') return inputPath;
    
    std::string baseDir = Ext2Utils::getDiskDir();
    return baseDir + inputPath;
}

// ============================================================
// Calcula el número de partición según su POSICIÓN en el disco.
// 
// Regla:
//   - Primarias (idx 0-3 en MBR) → números 1-4
//   - Lógicas (en cadena EBR)    → números (primarias montables) + (idx lógico + 1)
//   - Extendidas                  → NO se cuentan (no se montan)
//   - El EBR "placeholder" (el que tiene el nombre de la extendida) se SALTA
// ============================================================
static int calculatePartitionNumber(const MBR& mbr, std::fstream& disk,
                                     char partType, int mbrIndex, int64_t ebrPos) {
    // Si es primaria → posición = índice MBR + 1
    if (partType == 'P') {
        return mbrIndex + 1;
    }
    
    // Si es lógica → contar primarias montables + posición en la cadena EBR (saltando placeholder)
    if (partType == 'L') {
        // 1. Contar primarias montables (excluir extendidas)
        int primariasMontables = 0;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_s > 0 && mbr.mbr_partitions[i].part_type == 'P') {
                primariasMontables++;
            }
        }
        
        // 2. Encontrar la partición extendida
        int extendedSlot = -1;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_type == 'E' && mbr.mbr_partitions[i].part_s > 0) {
                extendedSlot = i;
                break;
            }
        }
        
        if (extendedSlot == -1) return primariasMontables + 1;
        
        // 3. Obtener el nombre de la extendida (para saltar el placeholder)
        std::string extendedName(mbr.mbr_partitions[extendedSlot].part_name);
        extendedName = extendedName.c_str();
        
        // 4. Saltar el EBR placeholder
        int64_t extendedStart = mbr.mbr_partitions[extendedSlot].part_start;
        int64_t currentPos = extendedStart;
        
        EBR ebr;
        disk.seekg(currentPos, std::ios::beg);
        disk.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
        
        std::string placeholderName(ebr.part_name);
        placeholderName = placeholderName.c_str();
        
        // Si el primer EBR es el placeholder (tiene el nombre de la extendida), saltarlo
        if (placeholderName == extendedName) {
            if (ebr.part_next == -1) {
                // No hay lógicas, no debería llegar aquí
                return primariasMontables + 1;
            }
            currentPos = ebr.part_next;
        }
        
        // 5. Iterar sobre los EBRs reales (lógicas)
        int ebrIndex = 0;
        while (true) {
            if (currentPos == ebrPos) {
                return primariasMontables + ebrIndex + 1;
            }
            
            disk.seekg(currentPos, std::ios::beg);
            disk.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
            
            if (!disk.good() || ebr.part_s == 0) break;
            if (ebr.part_next == -1) break;
            
            currentPos = ebr.part_next;
            ebrIndex++;
        }
    }
    
    return 1;  // fallback
}

// ============================================================
// Genera el ID de montaje: últimos2Dígitos + numParticion + letra
// ============================================================
static std::string generateMountId(const std::string& carnet,
                                    const std::string& diskPath,
                                    int partitionNumber,
                                    const std::map<std::string, std::string>& mountedDisks) {
    std::string lastTwo = carnet.substr(carnet.length() - 2);
    
    // 1. Verificar si este disco ya tiene una letra asignada
    char diskLetter = '\0';
    for (const auto& entry : mountedDisks) {
        if (entry.second == diskPath) {
            const std::string& existingId = entry.first;
            if (existingId.length() >= 4) {
                diskLetter = existingId.back();
                break;
            }
        }
    }
    
    // 2. Si no tiene letra, asignar la siguiente disponible
    if (diskLetter == '\0') {
        std::set<char> usedLetters;
        for (const auto& entry : mountedDisks) {
            const std::string& id = entry.first;
            if (id.length() >= 4) {
                usedLetters.insert(id.back());
            }
        }
        diskLetter = 'A';
        while (usedLetters.count(diskLetter)) {
            diskLetter++;
        }
    }
    
    // 3. Construir el ID
    std::string mountId = lastTwo + std::to_string(partitionNumber) + diskLetter;
    
    // 4. Máximo 4 caracteres
    if (mountId.length() > 4) {
        mountId = mountId.substr(0, 4);
    }
    
    return mountId;
}

// ============================================================
// Busca una partición (primaria o lógica) por nombre
// ============================================================
struct PartitionSearchResult {
    char type = '\0';           // 'P', 'L', 'E', o '\0'
    int mbrIndex = -1;          // Índice en MBR (si es P o E)
    int64_t ebrPos = -1;        // Posición del EBR (si es L)
};

static PartitionSearchResult findPartitionByName(std::fstream& disk, 
                                                   const MBR& mbr,
                                                   const std::string& name) {
    PartitionSearchResult result;
    
    // 1. Buscar en MBR (primarias y extendida)
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_s <= 0) continue;
        
        std::string partName(mbr.mbr_partitions[i].part_name);
        partName = partName.c_str();
        
        if (partName == name) {
            result.type = mbr.mbr_partitions[i].part_type;
            result.mbrIndex = i;
            return result;
        }
    }
    
    // 2. Buscar en EBRs (lógicas)
    int extendedSlot = -1;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_type == 'E' && mbr.mbr_partitions[i].part_s > 0) {
            extendedSlot = i;
            break;
        }
    }
    
    if (extendedSlot == -1) return result;
    
    // ✅ Obtener nombre de la extendida para saltar el placeholder
    std::string extendedName(mbr.mbr_partitions[extendedSlot].part_name);
    extendedName = extendedName.c_str();
    
    int64_t extendedStart = mbr.mbr_partitions[extendedSlot].part_start;
    int64_t currentPos = extendedStart;
    
    while (true) {
        EBR ebr;
        disk.seekg(currentPos, std::ios::beg);
        disk.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
        
        if (!disk.good() || ebr.part_s == 0) break;
        
        std::string ebrName(ebr.part_name);
        ebrName = ebrName.c_str();
        
        // ✅ Saltar el placeholder si tiene el nombre de la extendida
        if (ebrName == extendedName) {
            if (ebr.part_next == -1) break;
            currentPos = ebr.part_next;
            continue;
        }
        
        if (ebrName == name) {
            result.type = 'L';
            result.ebrPos = currentPos;
            return result;
        }
        
        if (ebr.part_next == -1) break;
        currentPos = ebr.part_next;
    }
    
    return result;
}

// ============================================================
// MOUNT (principal)
// ============================================================
CommandResult CommandHandler::processMount(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        std::string rawPath = params["path"];
        std::string path = resolveDiskPath(rawPath);
        std::string name = params["name"];
        
        if (!validateDiskExists(path)) {
            result.message = "Error: El disco no existe en la ruta: " + path;
            return result;
        }
        
        std::fstream disk(path, std::ios::in | std::ios::out | std::ios::binary);
        if (!disk.is_open()) {
            result.message = "Error: No se pudo abrir el disco: " + path;
            return result;
        }
        
        MBR mbr;
        disk.seekg(0, std::ios::beg);
        disk.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        
        // Buscar la partición
        PartitionSearchResult found = findPartitionByName(disk, mbr, name);
        
        if (found.type == '\0') {
            disk.close();
            result.message = "Error: No existe la partición: " + name + " en el disco: " + path;
            return result;
        }
        
        // ✅ Rechazar extendidas
        if (found.type == 'E') {
            disk.close();
            result.message = "Error: No se puede montar una partición extendida";
            return result;
        }
        
        // ✅ Verificar que no esté montada ya
        if (found.type == 'P') {
            if (mbr.mbr_partitions[found.mbrIndex].part_status == '1') {
                disk.close();
                result.message = "Error: La partición ya está montada: " + name;
                return result;
            }
        } else if (found.type == 'L') {
            EBR ebr;
            disk.seekg(found.ebrPos, std::ios::beg);
            disk.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
            if (ebr.part_mount == '1') {
                disk.close();
                result.message = "Error: La partición ya está montada: " + name;
                return result;
            }
        }
        
        // ✅ Calcular el número de partición por POSICIÓN
        int partitionNumber = calculatePartitionNumber(mbr, disk, 
                                                        found.type, 
                                                        found.mbrIndex, 
                                                        found.ebrPos);
        
        // ✅ Generar el ID
        std::string carnet = "202200081";
        std::string mountId = generateMountId(carnet, path, partitionNumber, mountedDisks);
        
        // ✅ Actualizar según el tipo
        if (found.type == 'P') {
            mbr.mbr_partitions[found.mbrIndex].part_status = '1';
            mbr.mbr_partitions[found.mbrIndex].part_correlative = partitionNumber;
            memset(mbr.mbr_partitions[found.mbrIndex].part_id, 0, 4);
            strncpy(mbr.mbr_partitions[found.mbrIndex].part_id, mountId.c_str(), 4);
            
            disk.seekp(0, std::ios::beg);
            disk.write(reinterpret_cast<const char*>(&mbr), sizeof(MBR));
            
        } else if (found.type == 'L') {
            EBR ebr;
            disk.seekg(found.ebrPos, std::ios::beg);
            disk.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
            
            ebr.part_mount = '1';
            
            disk.seekp(found.ebrPos, std::ios::beg);
            disk.write(reinterpret_cast<const char*>(&ebr), sizeof(EBR));
        }
        
        disk.close();
        
        mountedDisks[mountId] = path;
        
        result.success = true;
        result.message = "Partición montada exitosamente. ID: " + mountId;
        result.data["mount"] = {
            {"id", mountId},
            {"path", path},
            {"name", name},
            {"type", std::string(1, found.type)},
            {"partitionNumber", partitionNumber}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar MOUNT: " + std::string(e.what());
    }
    
    return result;
}