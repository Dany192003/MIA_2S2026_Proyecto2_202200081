#include "command_handler.h"
#include "../utils/ext2_utils.h"
#include <fstream>
#include <cstring>

CommandResult CommandHandler::processLsjson(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        // 1. Obtener parámetros
        std::string path = params["path"];
        std::string id = params["id"];
        
        // 2. Validar que haya sesión activa
        if (!isLoggedIn()) {
            result.message = "Error: No hay sesión activa. Use LOGIN primero.";
            return result;
        }
        
        // 3. Validar ID
        if (mountedDisks.find(id) == mountedDisks.end()) {
            result.message = "Error: El ID de montaje no existe: " + id;
            return result;
        }
        
        // 4. Obtener la ruta del disco
        std::string diskPath = mountedDisks[id];
        
        // 5. Leer el MBR
        std::fstream disk(diskPath, std::ios::in | std::ios::binary);
        if (!disk.is_open()) {
            result.message = "Error: No se pudo abrir el disco: " + diskPath;
            return result;
        }
        
        MBR mbr;
        disk.seekg(0, std::ios::beg);
        disk.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        
        // 6. Buscar la partición por ID
        int partitionIndex = -1;
        for (int i = 0; i < 4; i++) {
            char partId[5] = {0};
            memcpy(partId, mbr.mbr_partitions[i].part_id, 4);
            partId[4] = '\0';
            std::string partIdStr(partId);
            partIdStr = partIdStr.c_str();
            
            if (partIdStr == id && mbr.mbr_partitions[i].part_s > 0) {
                partitionIndex = i;
                break;
            }
        }
        
        if (partitionIndex == -1) {
            disk.close();
            result.message = "Error: No se encontró la partición para el ID: " + id;
            return result;
        }
        
        disk.close();
        
        // 7. Leer el Superblock
        Superblock sb = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
        
        // 8. Encontrar el inodo de la ruta
        int inodeIndex = Ext2Utils::findInodeByPath(diskPath, path, sb, mbr, partitionIndex);
        if (inodeIndex == -1) {
            result.message = "Error: La ruta no existe: " + path;
            return result;
        }
        
        // 9. Leer el inodo
        std::fstream diskRead(diskPath, std::ios::in | std::ios::binary);
        if (!diskRead.is_open()) {
            result.message = "Error: No se pudo abrir el disco";
            return result;
        }
        
        Inode inode = Ext2Utils::readInode(diskRead, sb, inodeIndex);
        diskRead.close();
        
        // 10. Verificar que sea una carpeta
        if (inode.i_type != '0') {
            result.message = "Error: La ruta no es una carpeta: " + path;
            return result;
        }
        
        // 11. Recorrer los bloques de la carpeta
        json fileList = json::array();
        
        for (int i = 0; i < 12 && i < 16; i++) {
            if (inode.i_block[i] == -1) continue;
            
            std::fstream diskBlock(diskPath, std::ios::in | std::ios::binary);
            if (!diskBlock.is_open()) continue;
            
            BlockFolder block = Ext2Utils::readBlockFolder(diskBlock, sb, inode.i_block[i]);
            diskBlock.close();
            
            for (int j = 0; j < 4; j++) {
                std::string name = std::string(block.b_content[j].b_name, 12).c_str();
                if (name.empty() || name == "." || name == "..") continue;
                if (block.b_content[j].b_inodo == -1) continue;
                
                // Leer el inodo del archivo/carpeta
                std::fstream diskChild(diskPath, std::ios::in | std::ios::binary);
                if (!diskChild.is_open()) continue;
                
                Inode childInode = Ext2Utils::readInode(diskChild, sb, block.b_content[j].b_inodo);
                diskChild.close();
                
                json item;
                item["name"] = name;
                item["isFolder"] = (childInode.i_type == '0');
                item["size"] = childInode.i_s;
                item["uid"] = childInode.i_uid;
                item["gid"] = childInode.i_gid;
                item["perms"] = std::string(childInode.i_perm, 3);
                
                fileList.push_back(item);
            }
        }
        
        result.data["files"] = fileList;
        result.data["path"] = path;
        result.data["count"] = fileList.size();
        result.message = "Contenido de " + path + ": " + std::to_string(fileList.size()) + " elementos";
        result.success = true;
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar LSJSON: " + std::string(e.what());
    }
    
    return result;
}
