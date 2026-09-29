#include "command_handler.h"
#include "../utils/ext2_utils.h"

CommandResult CommandHandler::processMkgrp(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        // 1. Obtener parámetros
        std::string name = params["name"];
        
        // 2. Validar que haya sesión activa
        if (!isLoggedIn()) {
            result.message = "Error: No hay sesión activa. Use LOGIN primero.";
            return result;
        }
        
        // 3. Validar que sea usuario root
        if (!isRoot()) {
            result.message = "Error: Solo el usuario root puede crear grupos";
            return result;
        }
        
        // 4. Validar nombre (máximo 10 caracteres)
        if (name.length() > 10) {
            result.message = "Error: El nombre del grupo no puede exceder 10 caracteres";
            return result;
        }
        
        // 5. Obtener la ruta del disco y el ID de montaje
        std::string diskPath = currentSession.diskPath;
        std::string mountId = currentSession.mountId;
        
        // 6. Leer el MBR
        std::fstream disk(diskPath, std::ios::in | std::ios::out | std::ios::binary);
        if (!disk.is_open()) {
            result.message = "Error: No se pudo abrir el disco: " + diskPath;
            return result;
        }
        
        MBR mbr;
        disk.seekg(0, std::ios::beg);
        disk.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        
        // 7. Buscar la partición
        int partitionIndex = -1;
        for (int i = 0; i < 4; i++) {
            char partId[5] = {0};
            memcpy(partId, mbr.mbr_partitions[i].part_id, 4);
            partId[4] = '\0';
            std::string partIdStr(partId);
            partIdStr = partIdStr.c_str();
            
            if (partIdStr == mountId && mbr.mbr_partitions[i].part_s > 0) {
                partitionIndex = i;
                break;
            }
        }
        
        if (partitionIndex == -1) {
            result.message = "Error: No se encontró la partición para el ID: " + mountId;
            return result;
        }
        
        disk.close();
        
        // 8. Leer users.txt
        std::vector<std::string> userLines = Ext2Utils::readUsersFile(diskPath, mountId, mbr, partitionIndex);
        
        // 9. Verificar que el grupo no exista
        int nextGid = 2;
        for (const auto& line : userLines) {
            if (line.find(", G, ") != std::string::npos) {
                std::stringstream ss(line);
                std::string token;
                std::vector<std::string> parts;
                while (std::getline(ss, token, ',')) {
                    parts.push_back(token);
                }
                if (parts.size() >= 3) {
                    // Eliminar espacios
                    for (auto& p : parts) {
                        p.erase(0, p.find_first_not_of(" "));
                        p.erase(p.find_last_not_of(" ") + 1);
                    }
                    
                    std::string groupName = parts[2];
                    int gid = std::stoi(parts[0]);
                    
                    if (groupName == name && gid != 0) {
                        result.message = "Error: El grupo ya existe: " + name;
                        return result;
                    }
                    
                    if (gid >= nextGid) {
                        nextGid = gid + 1;
                    }
                }
            }
        }
        
        // 10. Agregar el grupo
        std::string newGroupLine = std::to_string(nextGid) + ", G, " + name;
        userLines.push_back(newGroupLine);
        
        // 11. Guardar users.txt
        if (!Ext2Utils::writeUsersFile(diskPath, userLines, mbr, partitionIndex)) {
            result.message = "Error: No se pudo guardar el grupo en users.txt";
            return result;
        }
        
        // 12. Éxito
        result.success = true;
        result.message = "Grupo creado exitosamente: " + name;
        result.data["group"] = {
            {"name", name},
            {"gid", nextGid}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar MKGRP: " + std::string(e.what());
    }
    
    return result;
}