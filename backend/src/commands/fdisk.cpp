#include "command_handler.h"
#include "../utils/ext2_utils.h"
#include <fstream>
#include <cstring>
#include <cmath>
#include <vector>
#include "../structures/ebr.h"

// ✅ NUEVO: Resuelve una ruta relativa contra EXT2_DISK_DIR
static std::string resolveDiskPath(const std::string& inputPath) {
    if (inputPath.empty()) return inputPath;
    if (inputPath[0] == '/') return inputPath;
    
    std::string baseDir = Ext2Utils::getDiskDir();
    return baseDir + inputPath;
}

static int64_t getFileSize(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return -1;
    return file.tellg();
}

static bool readEBR(std::fstream& disk, int64_t position, EBR& ebr) {
    disk.seekg(position, std::ios::beg);
    disk.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
    return disk.good();
}

static bool writeEBR(std::fstream& disk, int64_t position, const EBR& ebr) {
    disk.seekp(position, std::ios::beg);
    disk.write(reinterpret_cast<const char*>(&ebr), sizeof(EBR));
    return disk.good();
}

// Validar nombre en MBR Y EBRs
static bool isPartitionNameUniqueAcrossAll(const MBR& mbr, std::fstream& disk, const std::string& name, int extendedSlot) {
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_s > 0) {
            std::string partName(mbr.mbr_partitions[i].part_name);
            partName = partName.c_str();
            if (partName == name) return false;
        }
    }
    
    if (extendedSlot != -1) {
        int64_t extendedStart = mbr.mbr_partitions[extendedSlot].part_start;
        EBR ebr;
        int64_t currentPos = extendedStart;
        
        while (true) {
            if (!readEBR(disk, currentPos, ebr)) break;
            if (ebr.part_s == 0) break;
            
            std::string ebrName(ebr.part_name);
            ebrName = ebrName.c_str();
            if (ebrName == name) return false;
            
            if (ebr.part_next == -1) break;
            currentPos = ebr.part_next;
        }
    }
    
    return true;
}

// ============================================================
// FDISK DELETE
// Elimina una partición (fast: solo marca vacío / full: + rellena con \0)
// ============================================================
static CommandResult fdiskDelete(std::fstream& disk, MBR& mbr, const std::string& path,
                                  const std::string& name, const std::string& deleteMode,
                                  int64_t diskSize) {
    CommandResult result;
    result.success = false;
    
    // 1. Buscar la partición en el MBR (primarias + extendida)
    int partitionIndex = -1;
    for (int i = 0; i < 4; i++) {
        std::string partName(mbr.mbr_partitions[i].part_name);
        partName = partName.c_str();
        if (partName == name && mbr.mbr_partitions[i].part_s > 0) {
            partitionIndex = i;
            break;
        }
    }
    
    // 2. Si no está en MBR, buscar en EBRs (lógicas)
    if (partitionIndex == -1) {
        int extendedSlot = -1;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_type == 'E' && mbr.mbr_partitions[i].part_s > 0) {
                extendedSlot = i;
                break;
            }
        }
        
        if (extendedSlot != -1) {
            int64_t extendedStart = mbr.mbr_partitions[extendedSlot].part_start;
            EBR ebr;
            int64_t currentPos = extendedStart;
            int64_t prevPos = -1;
            
            while (true) {
                if (!readEBR(disk, currentPos, ebr)) break;
                if (ebr.part_s == 0) break;
                
                std::string ebrName(ebr.part_name);
                ebrName = ebrName.c_str();
                
                if (ebrName == name) {
                    // Encontrada lógica
                    int64_t startDelete = currentPos;
                    int64_t sizeDelete = sizeof(EBR) + ebr.part_s;
                    
                    // Desenlazar de la lista
                    if (prevPos == -1) {
                        // Es la primera lógica: resetear el EBR de la extendida
                        EBR firstEbr;
                        if (readEBR(disk, extendedStart, firstEbr)) {
                            firstEbr.part_s = 0;
                            firstEbr.part_next = -1;
                            firstEbr.part_start = extendedStart + sizeof(EBR);
                            writeEBR(disk, extendedStart, firstEbr);
                        }
                    } else {
                        // Es una lógica intermedia: actualizar part_next del anterior
                        EBR prevEbr;
                        if (readEBR(disk, prevPos, prevEbr)) {
                            prevEbr.part_next = ebr.part_next;
                            writeEBR(disk, prevPos, prevEbr);
                        }
                    }
                    
                    // Si es FULL, rellenar con \0
                    if (deleteMode == "full") {
                        char zero[1024] = {0};
                        disk.seekp(startDelete, std::ios::beg);
                        int64_t written = 0;
                        while (written < sizeDelete) {
                            int64_t chunk = std::min((int64_t)1024, sizeDelete - written);
                            disk.write(zero, chunk);
                            written += chunk;
                        }
                    }
                    
                    result.success = true;
                    result.message = "Partición lógica eliminada (" + deleteMode + "): " + name;
                    result.data["deleted"] = {
                        {"name", name},
                        {"type", "L"},
                        {"mode", deleteMode},
                        {"start", startDelete},
                        {"size", sizeDelete}
                    };
                    return result;
                }
                
                prevPos = currentPos;
                if (ebr.part_next == -1) break;
                currentPos = ebr.part_next;
            }
        }
        
        result.message = "Error: No existe la partición: " + name;
        return result;
    }
    
    // 3. Es una partición del MBR
    Partition& part = mbr.mbr_partitions[partitionIndex];
    int64_t partStart = part.part_start;
    int64_t partSize = part.part_s;
    char partType = part.part_type;
    
    // 4. Si es extendida, también hay que eliminar las lógicas internas
    if (partType == 'E') {
        EBR ebr;
        int64_t currentPos = partStart;
        
        while (true) {
            if (!readEBR(disk, currentPos, ebr)) break;
            if (ebr.part_s == 0) break;
            
            if (deleteMode == "full") {
                char zero[1024] = {0};
                disk.seekp(currentPos, std::ios::beg);
                int64_t sizeEbr = sizeof(EBR) + ebr.part_s;
                int64_t written = 0;
                while (written < sizeEbr) {
                    int64_t chunk = std::min((int64_t)1024, sizeEbr - written);
                    disk.write(zero, chunk);
                    written += chunk;
                }
            }
            
            if (ebr.part_next == -1) break;
            currentPos = ebr.part_next;
        }
    }
    
    // 5. Si es FULL, rellenar el espacio de la partición con \0
    if (deleteMode == "full") {
        char zero[1024] = {0};
        disk.seekp(partStart, std::ios::beg);
        int64_t written = 0;
        while (written < partSize) {
            int64_t chunk = std::min((int64_t)1024, partSize - written);
            disk.write(zero, chunk);
            written += chunk;
        }
    }
    
    // 6. Marcar como vacía en el MBR
    memset(&mbr.mbr_partitions[partitionIndex], 0, sizeof(Partition));
    mbr.mbr_partitions[partitionIndex].part_status = '0';
    mbr.mbr_partitions[partitionIndex].part_type = 'P';
    mbr.mbr_partitions[partitionIndex].part_start = -1;
    mbr.mbr_partitions[partitionIndex].part_s = 0;
    mbr.mbr_partitions[partitionIndex].part_correlative = -1;
    
    // 7. Escribir MBR actualizado
    disk.seekp(0, std::ios::beg);
    disk.write(reinterpret_cast<const char*>(&mbr), sizeof(MBR));
    
    result.success = true;
    result.message = "Partición eliminada (" + deleteMode + "): " + name;
    result.data["deleted"] = {
        {"name", name},
        {"type", std::string(1, partType)},
        {"mode", deleteMode},
        {"start", partStart},
        {"size", partSize}
    };
    
    return result;
}

// ============================================================
// FDISK ADD
// Agrega (+) o quita (-) espacio a una partición
// ============================================================
static CommandResult fdiskAdd(std::fstream& disk, MBR& mbr, const std::string& path,
                               const std::string& name, int64_t addBytes,
                               int64_t diskSize) {
    CommandResult result;
    result.success = false;
    
    // 1. Buscar la partición en el MBR
    int partitionIndex = -1;
    for (int i = 0; i < 4; i++) {
        std::string partName(mbr.mbr_partitions[i].part_name);
        partName = partName.c_str();
        if (partName == name && mbr.mbr_partitions[i].part_s > 0) {
            partitionIndex = i;
            break;
        }
    }
    
    // 2. Si no está en MBR, buscar en EBRs (lógicas)
    if (partitionIndex == -1) {
        int extendedSlot = -1;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_type == 'E' && mbr.mbr_partitions[i].part_s > 0) {
                extendedSlot = i;
                break;
            }
        }
        
        if (extendedSlot != -1) {
            int64_t extendedStart = mbr.mbr_partitions[extendedSlot].part_start;
            int64_t extendedSize = mbr.mbr_partitions[extendedSlot].part_s;
            int64_t extendedEnd = extendedStart + extendedSize;
            
            EBR ebr;
            int64_t currentPos = extendedStart;
            
            while (true) {
                if (!readEBR(disk, currentPos, ebr)) break;
                if (ebr.part_s == 0) break;
                
                std::string ebrName(ebr.part_name);
                ebrName = ebrName.c_str();
                
                if (ebrName == name) {
                    // Encontrada lógica
                    int64_t newSize = ebr.part_s + addBytes;
                    
                    if (newSize <= 0) {
                        result.message = "Error: El tamaño de la partición no puede ser negativo o cero";
                        return result;
                    }
                    
                    if (addBytes > 0) {
                        // Validar que quepa en la extendida
                        int64_t nextPos = ebr.part_start + ebr.part_s + addBytes;
                        if (nextPos > extendedEnd) {
                            result.message = "Error: No hay espacio suficiente en la partición extendida";
                            return result;
                        }
                    }
                    
                    ebr.part_s = newSize;
                    writeEBR(disk, currentPos, ebr);
                    
                    result.success = true;
                    result.message = "Espacio " + std::string(addBytes > 0 ? "agregado" : "quitado") +
                                     " exitosamente a: " + name;
                    result.data["add"] = {
                        {"name", name},
                        {"type", "L"},
                        {"add", addBytes},
                        {"newSize", newSize}
                    };
                    return result;
                }
                
                if (ebr.part_next == -1) break;
                currentPos = ebr.part_next;
            }
        }
        
        result.message = "Error: No existe la partición: " + name;
        return result;
    }
    
    // 3. Es una partición del MBR
    Partition& part = mbr.mbr_partitions[partitionIndex];
    int64_t newSize = part.part_s + addBytes;
    
    if (newSize <= 0) {
        result.message = "Error: El tamaño de la partición no puede ser negativo o cero";
        return result;
    }
    
    if (addBytes > 0) {
        // Validar que no sobrepase el disco o la siguiente partición
        int64_t partEnd = part.part_start + newSize;
        if (partEnd > diskSize) {
            result.message = "Error: No hay espacio suficiente en el disco (excede el final)";
            return result;
        }
        
        // Validar que no choque con la siguiente partición
        for (int i = 0; i < 4; i++) {
            if (i == partitionIndex) continue;
            if (mbr.mbr_partitions[i].part_s == 0) continue;
            
            int64_t otherStart = mbr.mbr_partitions[i].part_start;
            if (otherStart >= part.part_start && otherStart < partEnd) {
                result.message = "Error: No hay espacio libre después de la partición (choca con: " +
                                 std::string(mbr.mbr_partitions[i].part_name) + ")";
                return result;
            }
        }
    }
    
    part.part_s = newSize;
    
    // Escribir MBR actualizado
    disk.seekp(0, std::ios::beg);
    disk.write(reinterpret_cast<const char*>(&mbr), sizeof(MBR));
    
    result.success = true;
    result.message = "Espacio " + std::string(addBytes > 0 ? "agregado" : "quitado") +
                     " exitosamente a: " + name;
    result.data["add"] = {
        {"name", name},
        {"type", std::string(1, part.part_type)},
        {"add", addBytes},
        {"newSize", newSize}
    };
    
    return result;
}

// ============================================================
// FDISK (principal)
// ============================================================
CommandResult CommandHandler::processFdisk(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        // 1. Parámetros obligatorios
        std::string rawPath = params["path"];
        // ✅ NUEVO: Resolver ruta relativa contra EXT2_DISK_DIR
        std::string path = resolveDiskPath(rawPath);
        std::string name = params["name"];
        
        // 2. Parámetros opcionales
        bool hasAdd = params.contains("add");
        bool hasDelete = params.contains("delete");
        bool hasSize = params.contains("size");
        
        // 3. Validar el disco
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
        
        int64_t diskSize = getFileSize(path);
        if (diskSize == -1) {
            disk.close();
            result.message = "Error: No se pudo obtener el tamaño del disco";
            return result;
        }
        
        // ============================================================
        // CASO 1: DELETE
        // ============================================================
        if (hasDelete) {
            std::string deleteMode = params["delete"];
            std::transform(deleteMode.begin(), deleteMode.end(), deleteMode.begin(), ::tolower);
            
            if (deleteMode != "fast" && deleteMode != "full") {
                disk.close();
                result.message = "Error: Modo de delete inválido. Use 'fast' o 'full'";
                return result;
            }
            
            CommandResult delResult = fdiskDelete(disk, mbr, path, name, deleteMode, diskSize);
            disk.close();
            return delResult;
        }
        
        // ============================================================
        // CASO 2: ADD
        // ============================================================
        if (hasAdd) {
            std::string addStr = params["add"];
            int addValue;
            try {
                addValue = std::stoi(addStr);
            } catch (...) {
                disk.close();
                result.message = "Error: El valor de -add debe ser un número entero";
                return result;
            }
            
            if (addValue == 0) {
                disk.close();
                result.message = "Error: El valor de -add no puede ser 0";
                return result;
            }
            
            // Determinar unidades (default K)
            std::string unit = params.contains("unit") ? std::string(params["unit"]) : "k";
            std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);
            
            int64_t addBytes;
            if (unit == "b") addBytes = addValue;
            else if (unit == "k") addBytes = (int64_t)addValue * 1024;
            else if (unit == "m") addBytes = (int64_t)addValue * 1024 * 1024;
            else {
                disk.close();
                result.message = "Error: Unidad inválida. Use B, K o M";
                return result;
            }
            
            CommandResult addResult = fdiskAdd(disk, mbr, path, name, addBytes, diskSize);
            disk.close();
            return addResult;
        }
        
        // ============================================================
        // CASO 3: CREATE
        // ============================================================
        if (!hasSize) {
            disk.close();
            result.message = "Error: Debe especificar -size para crear una partición";
            return result;
        }
        
        int size = std::stoi(std::string(params["size"]));
        std::string unit = params.contains("unit") ? std::string(params["unit"]) : "k";
        std::string type = params.contains("type") ? std::string(params["type"]) : "p";
        std::string fit = params.contains("fit") ? std::string(params["fit"]) : "wf";
        
        std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);
        std::transform(type.begin(), type.end(), type.begin(), ::tolower);
        std::transform(fit.begin(), fit.end(), fit.begin(), ::tolower);
        
        if (size <= 0) {
            disk.close();
            result.message = "Error: El tamaño debe ser mayor que cero";
            return result;
        }
        
        char unitChar;
        if (unit == "b") unitChar = 'B';
        else if (unit == "k") unitChar = 'K';
        else if (unit == "m") unitChar = 'M';
        else {
            disk.close();
            result.message = "Error: Unidad inválida. Use B, K o M";
            return result;
        }
        
        char typeChar;
        if (type == "p") typeChar = 'P';
        else if (type == "e") typeChar = 'E';
        else if (type == "l") typeChar = 'L';
        else {
            disk.close();
            result.message = "Error: Tipo inválido. Use P, E o L";
            return result;
        }
        
        char fitChar;
        if (fit == "bf") fitChar = 'B';
        else if (fit == "ff") fitChar = 'F';
        else if (fit == "wf") fitChar = 'W';
        else {
            disk.close();
            result.message = "Error: Ajuste inválido. Use BF, FF o WF";
            return result;
        }
        
        int64_t sizeInBytes;
        if (unitChar == 'B') sizeInBytes = size;
        else if (unitChar == 'K') sizeInBytes = (int64_t)size * 1024;
        else sizeInBytes = (int64_t)size * 1024 * 1024;
        
        int freeSlot = -1;
        int primaryCount = 0;
        int extendedCount = 0;
        int extendedSlot = -1;
        int64_t usedSpace = sizeof(MBR);
        
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_s > 0) {
                usedSpace += mbr.mbr_partitions[i].part_s;
                if (mbr.mbr_partitions[i].part_type == 'P') primaryCount++;
                else if (mbr.mbr_partitions[i].part_type == 'E') {
                    extendedCount++;
                    extendedSlot = i;
                }
            } else {
                if (freeSlot == -1) freeSlot = i;
            }
        }
        
        if (!isPartitionNameUniqueAcrossAll(mbr, disk, name, extendedSlot)) {
            disk.close();
            result.message = "Error: El nombre de partición ya existe en este disco: " + name;
            return result;
        }
        
        if (typeChar == 'P') {
            if (primaryCount + extendedCount >= 4) {
                disk.close();
                result.message = "Error: Ya hay 4 particiones en el disco";
                return result;
            }
            if (freeSlot == -1) {
                disk.close();
                result.message = "Error: No hay espacio para más particiones";
                return result;
            }
            
            int64_t freeSpace = diskSize - usedSpace;
            if (sizeInBytes > freeSpace) {
                disk.close();
                result.message = "Error: No hay suficiente espacio libre en el disco";
                return result;
            }
            
            Partition newPartition;
            memset(&newPartition, 0, sizeof(Partition));
            newPartition.part_status = '0';
            newPartition.part_type = typeChar;
            newPartition.part_fit = fitChar;
            newPartition.part_start = usedSpace;
            newPartition.part_s = sizeInBytes;
            strncpy(newPartition.part_name, name.c_str(), 15);
            newPartition.part_correlative = -1;
            memset(newPartition.part_id, 0, 4);
            
            mbr.mbr_partitions[freeSlot] = newPartition;
            
            disk.seekp(0, std::ios::beg);
            disk.write(reinterpret_cast<const char*>(&mbr), sizeof(MBR));
            disk.close();
            
            result.success = true;
            result.message = "Partición creada exitosamente: " + name;
            result.data["partition"] = {
                {"name", name},
                {"size", size},
                {"unit", unit},
                {"type", "p"},
                {"fit", fit},
                {"start", usedSpace},
                {"bytes", sizeInBytes}
            };
            return result;
            
        } else if (typeChar == 'E') {
            if (extendedCount >= 1) {
                disk.close();
                result.message = "Error: Ya existe una partición extendida en el disco";
                return result;
            }
            if (primaryCount + extendedCount >= 4) {
                disk.close();
                result.message = "Error: No se puede crear partición extendida, ya hay 4 particiones";
                return result;
            }
            if (freeSlot == -1) {
                disk.close();
                result.message = "Error: No hay espacio para más particiones";
                return result;
            }
            
            int64_t freeSpace = diskSize - usedSpace;
            if (sizeInBytes > freeSpace) {
                disk.close();
                result.message = "Error: No hay suficiente espacio libre en el disco";
                return result;
            }
            
            Partition newPartition;
            memset(&newPartition, 0, sizeof(Partition));
            newPartition.part_status = '0';
            newPartition.part_type = typeChar;
            newPartition.part_fit = fitChar;
            newPartition.part_start = usedSpace;
            newPartition.part_s = sizeInBytes;
            strncpy(newPartition.part_name, name.c_str(), 15);
            newPartition.part_correlative = -1;
            memset(newPartition.part_id, 0, 4);
            
            mbr.mbr_partitions[freeSlot] = newPartition;
            
            EBR firstEbr;
            memset(&firstEbr, 0, sizeof(EBR));
            firstEbr.part_mount = '0';
            firstEbr.part_fit = fitChar;
            firstEbr.part_start = usedSpace + sizeof(EBR);
            firstEbr.part_s = 0;
            firstEbr.part_next = -1;
            strncpy(firstEbr.part_name, name.c_str(), 15);
            
            disk.seekp(usedSpace, std::ios::beg);
            disk.write(reinterpret_cast<const char*>(&firstEbr), sizeof(EBR));
            
            disk.seekp(0, std::ios::beg);
            disk.write(reinterpret_cast<const char*>(&mbr), sizeof(MBR));
            disk.close();
            
            result.success = true;
            result.message = "Partición extendida creada exitosamente: " + name;
            result.data["partition"] = {
                {"name", name},
                {"size", size},
                {"unit", unit},
                {"type", "e"},
                {"fit", fit},
                {"start", usedSpace},
                {"bytes", sizeInBytes}
            };
            return result;
            
        } else if (typeChar == 'L') {
            if (extendedSlot == -1) {
                disk.close();
                result.message = "Error: No existe partición extendida para crear una lógica";
                return result;
            }
            
            int64_t extendedStart = mbr.mbr_partitions[extendedSlot].part_start;
            int64_t extendedSize = mbr.mbr_partitions[extendedSlot].part_s;
            int64_t extendedEnd = extendedStart + extendedSize;
            
            EBR currentEbr;
            if (!readEBR(disk, extendedStart, currentEbr)) {
                disk.close();
                result.message = "Error: No se pudo leer el EBR de la partición extendida";
                return result;
            }
            
            int64_t lastEbrPos = extendedStart;
            int64_t nextFreePos = extendedStart + sizeof(EBR);
            int logicalCount = 0;
            
            if (currentEbr.part_s == 0) {
                lastEbrPos = extendedStart;
                nextFreePos = extendedStart + sizeof(EBR);
                logicalCount = 0;
            } else {
                int64_t currentPos = extendedStart;
                while (true) {
                    EBR tempEbr;
                    if (!readEBR(disk, currentPos, tempEbr)) break;
                    if (tempEbr.part_s == 0) break;
                    
                    logicalCount++;
                    lastEbrPos = currentPos;
                    nextFreePos = tempEbr.part_start + tempEbr.part_s;
                    
                    if (tempEbr.part_next == -1) break;
                    currentPos = tempEbr.part_next;
                }
            }
            
            int64_t ebrSize = sizeof(EBR);
            int64_t totalNeeded = ebrSize + sizeInBytes;
            if (nextFreePos + totalNeeded > extendedEnd) {
                disk.close();
                result.message = "Error: No hay suficiente espacio en la partición extendida";
                return result;
            }
            
            EBR newEbr;
            memset(&newEbr, 0, sizeof(EBR));
            newEbr.part_mount = '0';
            newEbr.part_fit = fitChar;
            newEbr.part_start = nextFreePos + ebrSize;
            newEbr.part_s = sizeInBytes;
            newEbr.part_next = -1;
            strncpy(newEbr.part_name, name.c_str(), 15);
            
            if (logicalCount > 0) {
                EBR lastEbr;
                if (readEBR(disk, lastEbrPos, lastEbr)) {
                    lastEbr.part_next = nextFreePos;
                    writeEBR(disk, lastEbrPos, lastEbr);
                }
            }
            
            disk.seekp(nextFreePos, std::ios::beg);
            disk.write(reinterpret_cast<const char*>(&newEbr), sizeof(EBR));
            
            if (logicalCount == 0) {
                EBR firstEbr;
                if (readEBR(disk, extendedStart, firstEbr)) {
                    firstEbr.part_s = sizeInBytes;
                    firstEbr.part_next = nextFreePos;
                    writeEBR(disk, extendedStart, firstEbr);
                }
            }
            
            disk.close();
            
            result.success = true;
            result.message = "Partición lógica creada exitosamente: " + name;
            result.data["partition"] = {
                {"name", name},
                {"size", size},
                {"unit", unit},
                {"type", "l"},
                {"fit", fit},
                {"start", nextFreePos + ebrSize},
                {"bytes", sizeInBytes}
            };
            return result;
        }
        
        disk.close();
        result.message = "Error: Tipo de partición no reconocido";
        return result;
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar FDISK: " + std::string(e.what());
        return result;
    }
}