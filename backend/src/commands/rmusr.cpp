#include "command_handler.h"
#include "../utils/ext2_utils.h"
#include <fstream>
#include <sstream>
#include <cstring>

CommandResult CommandHandler::processRmusr(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        // 1. Obtener parámetros
        std::string user = params["user"];
        
        // 2. Validar que haya sesión activa
        if (!isLoggedIn()) {
            result.message = "Error: No hay sesión activa. Use LOGIN primero.";
            return result;
        }
        
        // 3. Validar que sea usuario root
        if (!isRoot()) {
            result.message = "Error: Solo el usuario root puede eliminar usuarios";
            return result;
        }
        
        // 4. Validar que no sea el usuario root
        if (user == "root") {
            result.message = "Error: No se puede eliminar el usuario root";
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
        
        // 7. Buscar la partición por ID
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
            disk.close();
            result.message = "Error: No se encontró la partición para el ID: " + mountId;
            return result;
        }
        
        // 8. Leer users.txt
        std::vector<std::string> userLines = Ext2Utils::readUsersFile(diskPath, mountId, mbr, partitionIndex);
        
        // 9. Buscar y eliminar el usuario
        bool userFound = false;
        for (size_t i = 0; i < userLines.size(); i++) {
            const std::string& line = userLines[i];
            if (line.find(", U, ") != std::string::npos) {
                std::stringstream ss(line);
                std::string token;
                std::vector<std::string> parts;
                while (std::getline(ss, token, ',')) {
                    parts.push_back(token);
                }
                if (parts.size() >= 5) {
                    for (auto& p : parts) {
                        p.erase(0, p.find_first_not_of(" "));
                        p.erase(p.find_last_not_of(" ") + 1);
                    }
                    
                    std::string username = parts[3];
                    int uid = std::stoi(parts[0]);
                    
                    if (username == user && uid != 0) {
                        // Marcar como eliminado (UID = 0)
                        userLines[i] = "0, U, " + parts[2] + ", " + user + ", " + parts[4];
                        userFound = true;
                        break;
                    }
                }
            }
        }
        
        if (!userFound) {
            disk.close();
            result.message = "Error: El usuario no existe: " + user;
            return result;
        }
        
        // 10. Guardar users.txt
        if (!Ext2Utils::writeUsersFile(diskPath, userLines, mbr, partitionIndex)) {
            disk.close();
            result.message = "Error: No se pudo guardar el cambio en users.txt";
            return result;
        }
        
        disk.close();
        
        // 11. Éxito
        result.success = true;
        result.message = "Usuario eliminado exitosamente: " + user;
        result.data["user"] = {
            {"name", user},
            {"status", "eliminado"}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar RMUSR: " + std::string(e.what());
    }
    
    return result;
}