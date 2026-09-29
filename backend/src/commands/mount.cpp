#include "command_handler.h"
#include <fstream>
#include <cstring>
#include <set>

static std::string generateMountId(const std::string& carnet,
                                    const std::string& diskPath,
                                    const MBR& mbr,
                                    int partitionIndex,
                                    const std::map<std::string, std::string>& mountedDisks) {
    (void)mbr;
    (void)partitionIndex;
    
    // Últimos dos dígitos del carnet
    std::string lastTwo = carnet.substr(carnet.length() - 2);
    
    char diskLetter = '\0';
    for (const auto& entry : mountedDisks) {
        if (entry.second == diskPath) {
            const std::string& existingId = entry.first;
            if (existingId.length() >= 3) {
                diskLetter = existingId.back();
                break;
            }
        }
    }
    
    // Si no tiene letra, asignar la siguiente disponible
    if (diskLetter == '\0') {
        std::set<char> usedLetters;
        for (const auto& entry : mountedDisks) {
            const std::string& id = entry.first;
            if (id.length() >= 3) {
                usedLetters.insert(id.back());
            }
        }
        diskLetter = 'A';
        while (usedLetters.count(diskLetter)) {
            diskLetter++;
        }
    }
    
    //2. Calcular el número de partición para ESTE disco
    int partitionNumber = 1;
    for (const auto& entry : mountedDisks) {
        if (entry.second == diskPath) {
            partitionNumber++;
        }
    }
    
    //3. Construir el ID
    std::string mountId = lastTwo + std::to_string(partitionNumber) + diskLetter;
    
    //4. Asegurar longitud máxima de 4 caracteres
    if (mountId.length() > 4) {
        mountId = mountId.substr(0, 4);
    }
    
    return mountId;
}

CommandResult CommandHandler::processMount(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        std::string path = params["path"];
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
        
        int partitionIndex = -1;
        for (int i = 0; i < 4; i++) {
            std::string partName(mbr.mbr_partitions[i].part_name);
            partName = partName.c_str();
            if (partName == name && mbr.mbr_partitions[i].part_s > 0) {
                partitionIndex = i;
                break;
            }
        }
        
        if (partitionIndex == -1) {
            disk.close();
            result.message = "Error: No existe la partición: " + name + " en el disco: " + path;
            return result;
        }
        
        if (mbr.mbr_partitions[partitionIndex].part_type == 'E') {
            disk.close();
            result.message = "Error: No se puede montar una partición extendida";
            return result;
        }
        
        if (mbr.mbr_partitions[partitionIndex].part_status == '1') {
            disk.close();
            result.message = "Error: La partición ya está montada: " + name;
            return result;
        }
        
        std::string carnet = "202200081";
        std::string mountId = generateMountId(carnet, path, mbr, partitionIndex, mountedDisks);
        
        mbr.mbr_partitions[partitionIndex].part_status = '1';
        mbr.mbr_partitions[partitionIndex].part_correlative = partitionIndex + 1;
        memset(mbr.mbr_partitions[partitionIndex].part_id, 0, 4);
        strncpy(mbr.mbr_partitions[partitionIndex].part_id, mountId.c_str(), 4);
        
        disk.seekp(0, std::ios::beg);
        disk.write(reinterpret_cast<const char*>(&mbr), sizeof(MBR));
        disk.close();
        
        mountedDisks[mountId] = path;
        
        result.success = true;
        result.message = "Partición montada exitosamente. ID: " + mountId;
        result.data["mount"] = {
            {"id", mountId},
            {"path", path},
            {"name", name},
            {"partition", partitionIndex},
            {"correlative", partitionIndex + 1}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar MOUNT: " + std::string(e.what());
    }
    
    return result;
}