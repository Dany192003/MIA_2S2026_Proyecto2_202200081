#include "command_handler.h"
#include "../utils/ext2_utils.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <vector>
#include <functional>

// ============================================================
// Helpers de permisos
// ============================================================

static bool hasReadPermission(const std::string& perms, const std::string& user,
                               int uid, int gid, int fileUid, int fileGid) {
    if (user == "root") return true;
    
    char readBit;
    if (uid == fileUid) readBit = perms[0];
    else if (gid == fileGid) readBit = perms[1];
    else readBit = perms[2];
    
    int permValue = readBit - '0';
    return (permValue & 4) != 0;
}

static int getCurrentUserGid(const std::vector<std::string>& userLines, const std::string& username) {
    for (const auto& line : userLines) {
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
                if (parts[3] == username && std::stoi(parts[0]) != 0) {
                    std::string groupName = parts[2];
                    for (const auto& gline : userLines) {
                        if (gline.find(", G, ") != std::string::npos) {
                            std::stringstream gss(gline);
                            std::string gtoken;
                            std::vector<std::string> gparts;
                            while (std::getline(gss, gtoken, ',')) {
                                gparts.push_back(gtoken);
                            }
                            if (gparts.size() >= 3) {
                                for (auto& p : gparts) {
                                    p.erase(0, p.find_first_not_of(" "));
                                    p.erase(p.find_last_not_of(" ") + 1);
                                }
                                if (gparts[2] == groupName && std::stoi(gparts[0]) != 0) {
                                    return std::stoi(gparts[0]);
                                }
                            }
                        }
                    }
                    return 0;
                }
            }
        }
    }
    return 0;
}

// ============================================================
// ✅ Wildcard matching con ? y *
// ============================================================

// Función recursiva que compara un nombre contra un patrón con wildcards
static bool matchPattern(const std::string& name, const std::string& pattern) {
    size_t n = 0;  // índice en name
    size_t p = 0;  // índice en pattern
    size_t starIdx = std::string::npos;
    size_t matchIdx = 0;
    
    while (n < name.length()) {
        if (p < pattern.length() && (pattern[p] == '?' || pattern[p] == name[n])) {
            // Coincide carácter a carácter o con ?
            n++;
            p++;
        } else if (p < pattern.length() && pattern[p] == '*') {
            // Guardar posición de * y seguir
            starIdx = p;
            matchIdx = n;
            p++;
        } else if (starIdx != std::string::npos) {
            // Retroceder: * consume un carácter más del nombre
            p = starIdx + 1;
            matchIdx++;
            n = matchIdx;
        } else {
            return false;
        }
    }
    
    // Consumir * restantes al final del patrón
    while (p < pattern.length() && pattern[p] == '*') {
        p++;
    }
    
    return p == pattern.length();
}

// ============================================================
// Búsqueda recursiva desde un inodo
// ============================================================
static void findRecursive(std::fstream& disk, const Superblock& sb,
                           int inodeIndex, const std::string& currentPath,
                           const std::string& pattern,
                           int uid, int gid, const std::string& user,
                           json& results, int& count) {
    Inode inode = Ext2Utils::readInode(disk, sb, inodeIndex);
    if (inode.i_type != '0') return;
    
    // Verificar permiso de lectura sobre esta carpeta
    std::string perms(inode.i_perm, 3);
    if (!hasReadPermission(perms, user, uid, gid, inode.i_uid, inode.i_gid)) {
        return;
    }
    
    // Recorrer las entradas de la carpeta
    for (int b = 0; b < 12; b++) {
        if (inode.i_block[b] == -1) continue;
        
        BlockFolder block = Ext2Utils::readBlockFolder(disk, sb, inode.i_block[b]);
        
        for (int i = 0; i < 4; i++) {
            std::string name(block.b_content[i].b_name);
            name = name.c_str();
            
            if (name.empty() || name == "." || name == "..") continue;
            if (block.b_content[i].b_inodo == -1) continue;
            
            int childInode = block.b_content[i].b_inodo;
            Inode child = Ext2Utils::readInode(disk, sb, childInode);
            
            // Verificar permiso de lectura sobre el hijo
            std::string childPerms(child.i_perm, 3);
            if (!hasReadPermission(childPerms, user, uid, gid, child.i_uid, child.i_gid)) {
                continue;
            }
            
            // Construir el path completo
            std::string childPath = currentPath;
            if (childPath.back() != '/') childPath += "/";
            childPath += name;
            
            // Verificar si el nombre coincide con el patrón
            if (matchPattern(name, pattern)) {
                json item;
                item["name"] = name;
                item["path"] = childPath;
                item["isFolder"] = (child.i_type == '0');
                item["size"] = child.i_s;
                item["uid"] = child.i_uid;
                item["gid"] = child.i_gid;
                item["perms"] = std::string(child.i_perm, 3);
                results.push_back(item);
                count++;
            }
            
            // Si es carpeta, recursión
            if (child.i_type == '0') {
                findRecursive(disk, sb, childInode, childPath, pattern,
                               uid, gid, user, results, count);
            }
        }
    }
}

// ============================================================
// FIND
// ============================================================
CommandResult CommandHandler::processFind(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        // 1. Validar sesión
        if (!isLoggedIn()) {
            result.message = "Error: No hay sesión activa. Use LOGIN primero.";
            return result;
        }
        
        // 2. Validar parámetros
        if (!params.contains("path")) {
            result.message = "Error: El parámetro -path es obligatorio";
            return result;
        }
        if (!params.contains("name")) {
            result.message = "Error: El parámetro -name es obligatorio";
            return result;
        }
        
        std::string path = params["path"];
        std::string pattern = params["name"];
        
        if (path.empty() || path == "/") {
            path = "/";
        }
        
        if (pattern.empty()) {
            result.message = "Error: El patrón de búsqueda no puede estar vacío";
            return result;
        }
        
        // 3. Obtener info de sesión
        std::string diskPath = currentSession.diskPath;
        std::string mountId = currentSession.mountId;
        int uid = currentSession.uid;
        int gid = currentSession.gid;
        std::string user = currentSession.user;
        
        // 4. Abrir disco y leer MBR
        std::fstream disk(diskPath, std::ios::in | std::ios::out | std::ios::binary);
        if (!disk.is_open()) {
            result.message = "Error: No se pudo abrir el disco: " + diskPath;
            return result;
        }
        
        MBR mbr;
        disk.seekg(0, std::ios::beg);
        disk.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        
        // 5. Buscar la partición
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
        
        // 6. Leer superbloque
        Superblock sb = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
        
        // 7. Leer users.txt para obtener GID actual
        std::vector<std::string> userLines = Ext2Utils::readUsersFile(diskPath, mountId, mbr, partitionIndex);
        int currentGid = getCurrentUserGid(userLines, user);
        if (currentGid != 0) gid = currentGid;
        
        // 8. Validar que el path exista
        int startInode = Ext2Utils::findInodeByPathInternal(disk, path, sb);
        if (startInode == -1) {
            disk.close();
            result.message = "Error: No existe la ruta: " + path;
            return result;
        }
        
        // 9. Verificar que sea carpeta
        Inode startInodeStruct = Ext2Utils::readInode(disk, sb, startInode);
        if (startInodeStruct.i_type != '0') {
            disk.close();
            result.message = "Error: La ruta no es una carpeta: " + path;
            return result;
        }
        
        // 10. Buscar recursivamente
        json results = json::array();
        int count = 0;
        
        findRecursive(disk, sb, startInode, path, pattern,
                       uid, gid, user, results, count);
        
        disk.close();
        
        // 11. Éxito
        result.success = true;
        if (count == 0) {
            result.message = "No se encontraron resultados para: " + pattern;
        } else {
            result.message = "Se encontraron " + std::to_string(count) + " resultado(s) para: " + pattern;
        }
        result.data["find"] = {
            {"path", path},
            {"pattern", pattern},
            {"count", count},
            {"results", results}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar FIND: " + std::string(e.what());
    }
    
    return result;
}