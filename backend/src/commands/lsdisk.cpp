#include "command_handler.h"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <cstdlib>

namespace fs = std::filesystem;

// Ruta configurable con fallback absoluto
static std::string getDiskDir() {
    const char* env = std::getenv("EXT2_DISK_DIR");
    if (env && strlen(env) > 0) {
        return std::string(env);
    }
    
    return "/home/Edwin/Desktop/MIA_2S2026_Proyecto2_202200081/discos/";
}

// ✅ CORREGIDO: Recibe el nombre de la extendida para saltar su EBR
static void readLogicalPartitions(std::ifstream& diskFile, 
                                   int64_t extendedStart, 
                                   const std::string& extendedName,
                                   json& partitions, 
                                   int& partCount) {
    std::streampos currentPos = diskFile.tellg();
    
    EBR ebr;
    int64_t ebrPos = extendedStart;
    
    // Leer el primer EBR
    diskFile.seekg(ebrPos, std::ios::beg);
    diskFile.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
    
    // ✅ CORREGIDO: Saltar el EBR "de la extendida" si tiene el mismo nombre
    // Esto pasa cuando la extendida se creó pero aún no había lógicas
    std::string firstName(ebr.part_name, 16);
    size_t nul = firstName.find('\0');
    if (nul != std::string::npos) firstName = firstName.substr(0, nul);
    
    if (firstName == extendedName) {
        // Es el EBR de la extendida, saltarlo
        if (ebr.part_next != -1) {
            ebrPos = ebr.part_next;
        } else {
            // No hay siguiente, la extendida está vacía
            diskFile.seekg(currentPos, std::ios::beg);
            return;
        }
    }
    
    // Recorrer la cadena de EBRs reales (lógicas)
    while (true) {
        diskFile.seekg(ebrPos, std::ios::beg);
        diskFile.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
        if (!diskFile || ebr.part_s == 0) break;
        
        std::string partName(ebr.part_name, 16);
        nul = partName.find('\0');
        if (nul != std::string::npos) partName = partName.substr(0, nul);
        
        // ✅ CORREGIDO: Saltar si aún tiene el nombre de la extendida
        if (partName == extendedName) {
            if (ebr.part_next == -1) break;
            ebrPos = ebr.part_next;
            continue;
        }
        
        json part;
        part["name"] = partName;
        part["type"] = "L";
        part["size"] = ebr.part_s;
        part["start"] = ebr.part_start;
        part["status"] = std::string(1, ebr.part_mount);
        part["id"] = "";
        partitions.push_back(part);
        partCount++;
        
        if (ebr.part_next == -1) break;
        ebrPos = ebr.part_next;
    }
    
    diskFile.seekg(currentPos, std::ios::beg);
}

CommandResult CommandHandler::processLsdisk(const json& params) {
    (void)params;
    CommandResult result;
    result.success = false;
    
    try {
        std::string diskDir = getDiskDir();
        
        json diskList = json::array();
        
        if (!fs::exists(diskDir)) {
            result.message = "La carpeta de discos no existe: " + diskDir;
            result.data["disks"] = json::array();
            result.data["total"] = 0;
            result.success = true;
            return result;
        }
        
        for (const auto& entry : fs::directory_iterator(diskDir)) {
            if (entry.path().extension() == ".mia") {
                json diskInfo;
                diskInfo["name"] = entry.path().filename().string();
                diskInfo["path"] = entry.path().string();
                diskInfo["size"] = (int64_t)entry.file_size();
                
                std::ifstream diskFile(entry.path(), std::ios::binary);
                if (diskFile.is_open()) {
                    MBR mbr;
                    diskFile.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
                    
                    json partitions = json::array();
                    int partCount = 0;
                    
                    for (int i = 0; i < 4; i++) {
                        if (mbr.mbr_partitions[i].part_s > 0) {
                            std::string partName(mbr.mbr_partitions[i].part_name, 16);
                            size_t nul = partName.find('\0');
                            if (nul != std::string::npos) partName = partName.substr(0, nul);
                            
                            std::string partId(mbr.mbr_partitions[i].part_id, 4);
                            nul = partId.find('\0');
                            if (nul != std::string::npos) partId = partId.substr(0, nul);
                            
                            json part;
                            part["name"] = partName;
                            part["type"] = std::string(1, mbr.mbr_partitions[i].part_type);
                            part["size"] = mbr.mbr_partitions[i].part_s;
                            part["start"] = mbr.mbr_partitions[i].part_start;
                            part["status"] = std::string(1, mbr.mbr_partitions[i].part_status);
                            part["id"] = partId;
                            partitions.push_back(part);
                            partCount++;
                            
                            // ✅ CORREGIDO: Pasar el nombre de la extendida
                            if (mbr.mbr_partitions[i].part_type == 'E') {
                                readLogicalPartitions(diskFile, 
                                                     mbr.mbr_partitions[i].part_start,
                                                     partName,   // ← nombre de la extendida
                                                     partitions, 
                                                     partCount);
                            }
                        }
                    }
                    
                    diskFile.close();
                    
                    diskInfo["partitions"] = partitions;
                    diskInfo["partition_count"] = partCount;
                } else {
                    diskInfo["partitions"] = json::array();
                    diskInfo["partition_count"] = 0;
                }
                
                diskList.push_back(diskInfo);
            }
        }
        
        result.data["disks"] = diskList;
        result.data["total"] = diskList.size();
        result.message = "Discos encontrados: " + std::to_string(diskList.size());
        result.success = true;
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar LSDISK: " + std::string(e.what());
    }
    
    return result;
}