#include "command_handler.h"
#include "../utils/ext2_utils.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <cstring>

static bool hasWritePermission(const std::string& perms, const std::string& user, int uid, int gid, int fileUid, int fileGid) {
    char writeBit;
    
    if (user == "root") return true;
    
    if (uid == fileUid) {
        writeBit = perms[0];
    } else if (gid == fileGid) {
        writeBit = perms[1];
    } else {
        writeBit = perms[2];
    }
    
    int permValue = writeBit - '0';
    return (permValue & 2) != 0;
}

CommandResult CommandHandler::processMkdir(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        std::string path = params["path"];
        bool recursive = params.contains("p") && std::string(params["p"]) == "true";
        
        if (!isLoggedIn()) {
            result.message = "Error: No hay sesión activa. Use LOGIN primero.";
            return result;
        }
        
        if (path.empty()) {
            result.message = "Error: La ruta no puede estar vacía";
            return result;
        }
        
        // VALIDAR NOMBRE DE LA CARPETA (máximo 11 caracteres)
        size_t lastSlashPos = path.find_last_of('/');
        std::string dirName = (lastSlashPos != std::string::npos) ? path.substr(lastSlashPos + 1) : path;
        if (dirName.length() > 11) {
            result.message = "Error: El nombre no puede exceder 11 caracteres: " + dirName;
            return result;
        }
        
        std::string diskPath = currentSession.diskPath;
        std::string mountId = currentSession.mountId;
        int uid = currentSession.uid;
        int gid = currentSession.gid;
        std::string user = currentSession.user;
        
        std::fstream disk(diskPath, std::ios::in | std::ios::out | std::ios::binary);
        if (!disk.is_open()) {
            result.message = "Error: No se pudo abrir el disco: " + diskPath;
            return result;
        }
        
        MBR mbr;
        disk.seekg(0, std::ios::beg);
        disk.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        
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
        
        Superblock sb = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
        disk.close();
        
        // Verificar si la carpeta ya existe
        int existingInode = Ext2Utils::findInodeByPath(diskPath, path, sb, mbr, partitionIndex);
        if (existingInode != -1) {
            result.message = "Error: La carpeta ya existe en EXT2: " + path;
            return result;
        }
        
        // Verificar la carpeta padre
        if (lastSlashPos != std::string::npos) {
            std::string parentDir = path.substr(0, lastSlashPos);
            int parentInode = Ext2Utils::findInodeByPath(diskPath, parentDir, sb, mbr, partitionIndex);
            
            if (parentInode == -1) {
                if (recursive) {
                    // Crear carpetas padre recursivamente con permisos
                    std::vector<std::string> parts = Ext2Utils::splitPath(parentDir);
                    std::string currentPath = "/";
                    
                    for (const auto& part : parts) {
                        std::string testPath = currentPath + part;
                        int inode = Ext2Utils::findInodeByPath(diskPath, testPath, sb, mbr, partitionIndex);
                        
                        if (inode == -1) {
                            // Verificar permisos en la carpeta padre actual
                            if (currentPath != "/") {
                                int parentInodeCheck = Ext2Utils::findInodeByPath(diskPath, currentPath, sb, mbr, partitionIndex);
                                if (parentInodeCheck != -1) {
                                    std::fstream diskCheck(diskPath, std::ios::in | std::ios::binary);
                                    if (diskCheck.is_open()) {
                                        Inode parentInodeStruct = Ext2Utils::readInode(diskCheck, sb, parentInodeCheck);
                                        diskCheck.close();
                                        std::string perms(parentInodeStruct.i_perm, 3);
                                        if (!hasWritePermission(perms, user, uid, gid, parentInodeStruct.i_uid, parentInodeStruct.i_gid)) {
                                            result.message = "Error: No tienes permisos de escritura en: " + currentPath;
                                            return result;
                                        }
                                    }
                                }
                            }
                            
                            if (!Ext2Utils::createDirectory(diskPath, testPath, sb, mbr, partitionIndex, uid, gid)) {
                                result.message = "Error: No se pudo crear la carpeta en EXT2: " + testPath;
                                return result;
                            }
                            
                            // ✅ NUEVO: Registrar en journal
                            if (sb.s_filesystem_type == 3) {
                                writeJournal(sb, partitionIndex, "mkdir", testPath, "");
                            }
                        }
                        currentPath = testPath + "/";
                    }
                } else {
                    result.message = "Error: La carpeta padre no existe. Use -p para crearla automáticamente";
                    return result;
                }
            } else {
                // Verificar permisos en carpeta padre existente
                std::fstream diskRead(diskPath, std::ios::in | std::ios::binary);
                if (diskRead.is_open()) {
                    Inode parentInodeStruct = Ext2Utils::readInode(diskRead, sb, parentInode);
                    diskRead.close();
                    
                    std::string perms(parentInodeStruct.i_perm, 3);
                    if (!hasWritePermission(perms, user, uid, gid, parentInodeStruct.i_uid, parentInodeStruct.i_gid)) {
                        result.message = "Error: No tienes permisos de escritura en la carpeta padre: " + parentDir;
                        return result;
                    }
                }
            }
        }
        
        if (!Ext2Utils::createDirectory(diskPath, path, sb, mbr, partitionIndex, uid, gid)) {
            result.message = "Error: No se pudo crear la carpeta en EXT2: " + path;
            return result;
        }
        
        // ✅ NUEVO: Registrar en journal
        if (sb.s_filesystem_type == 3) {
            writeJournal(sb, partitionIndex, "mkdir", path, "");
        }
        
        result.success = true;
        result.message = "Carpeta creada exitosamente en EXT2: " + path;
        result.data["directory"] = {
            {"path", path},
            {"recursive", recursive},
            {"owner", currentSession.user},
            {"permissions", "664"}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar MKDIR: " + std::string(e.what());
    }
    
    return result;
}