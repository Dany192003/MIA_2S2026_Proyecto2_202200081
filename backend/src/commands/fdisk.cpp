#include "command_handler.h"
#include <fstream>
#include <cstring>
#include <cmath>
#include <vector>
#include "../structures/ebr.h"

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

//Validar nombre en MBR Y EBRs
static bool isPartitionNameUniqueAcrossAll(const MBR& mbr, std::fstream& disk, const std::string& name, int extendedSlot) {
    // 1. Revisar MBR (primarias + extendida)
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_s > 0) {
            std::string partName(mbr.mbr_partitions[i].part_name);
            partName = partName.c_str();
            if (partName == name) {
                return false;
            }
        }
    }
    
    // 2. Si existe extendida, revisar EBRs (lógicas)
    if (extendedSlot != -1) {
        int64_t extendedStart = mbr.mbr_partitions[extendedSlot].part_start;
        EBR ebr;
        int64_t currentPos = extendedStart;
        
        while (true) {
            if (!readEBR(disk, currentPos, ebr)) break;
            if (ebr.part_s == 0) break;
            
            std::string ebrName(ebr.part_name);
            ebrName = ebrName.c_str();
            if (ebrName == name) {
                return false;
            }
            
            if (ebr.part_next == -1) break;
            currentPos = ebr.part_next;
        }
    }
    
    return true;
}

CommandResult CommandHandler::processFdisk(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        std::string path = params["path"];
        std::string name = params["name"];
        int size = std::stoi(std::string(params["size"]));
        std::string unit = params.contains("unit") ? std::string(params["unit"]) : "k";
        std::string type = params.contains("type") ? std::string(params["type"]) : "p";
        std::string fit = params.contains("fit") ? std::string(params["fit"]) : "wf";
        
        std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);
        std::transform(type.begin(), type.end(), type.begin(), ::tolower);
        std::transform(fit.begin(), fit.end(), fit.begin(), ::tolower);
        
        if (size <= 0) {
            result.message = "Error: El tamaño debe ser mayor que cero";
            return result;
        }
        
        char unitChar;
        if (unit == "b") unitChar = 'B';
        else if (unit == "k") unitChar = 'K';
        else if (unit == "m") unitChar = 'M';
        else {
            result.message = "Error: Unidad inválida. Use B, K o M";
            return result;
        }
        
        char typeChar;
        if (type == "p") typeChar = 'P';
        else if (type == "e") typeChar = 'E';
        else if (type == "l") typeChar = 'L';
        else {
            result.message = "Error: Tipo inválido. Use P, E o L";
            return result;
        }
        
        char fitChar;
        if (fit == "bf") fitChar = 'B';
        else if (fit == "ff") fitChar = 'F';
        else if (fit == "wf") fitChar = 'W';
        else {
            result.message = "Error: Ajuste inválido. Use BF, FF o WF";
            return result;
        }
        
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
        
        int64_t sizeInBytes;
        if (unitChar == 'B') sizeInBytes = size;
        else if (unitChar == 'K') sizeInBytes = (int64_t)size * 1024;
        else sizeInBytes = (int64_t)size * 1024 * 1024;
        
        int64_t diskSize = getFileSize(path);
        if (diskSize == -1) {
            result.message = "Error: No se pudo obtener el tamaño del disco";
            return result;
        }
        
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
        
        //Validar nombre único en MBR + EBRs
        if (!isPartitionNameUniqueAcrossAll(mbr, disk, name, extendedSlot)) {
            disk.close();
            result.message = "Error: El nombre de partición ya existe en este disco: " + name;
            return result;
        }
        
        if (typeChar == 'P') {
            if (primaryCount + extendedCount >= 4) {
                result.message = "Error: Ya hay 4 particiones en el disco";
                return result;
            }
            if (freeSlot == -1) {
                result.message = "Error: No hay espacio para más particiones";
                return result;
            }
            
            int64_t freeSpace = diskSize - usedSpace;
            if (sizeInBytes > freeSpace) {
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
                result.message = "Error: Ya existe una partición extendida en el disco";
                return result;
            }
            if (primaryCount + extendedCount >= 4) {
                result.message = "Error: No se puede crear partición extendida, ya hay 4 particiones";
                return result;
            }
            if (freeSlot == -1) {
                result.message = "Error: No hay espacio para más particiones";
                return result;
            }
            
            int64_t freeSpace = diskSize - usedSpace;
            if (sizeInBytes > freeSpace) {
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
                result.message = "Error: No existe partición extendida para crear una lógica";
                return result;
            }
            
            int64_t extendedStart = mbr.mbr_partitions[extendedSlot].part_start;
            int64_t extendedSize = mbr.mbr_partitions[extendedSlot].part_s;
            int64_t extendedEnd = extendedStart + extendedSize;
            
            EBR currentEbr;
            if (!readEBR(disk, extendedStart, currentEbr)) {
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
        
        result.message = "Error: Tipo de partición no reconocido";
        return result;
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar FDISK: " + std::string(e.what());
        return result;
    }
}