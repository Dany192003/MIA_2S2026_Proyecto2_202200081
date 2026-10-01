#include "command_handler.h"
#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <cerrno>
#include <cstring>
#include <ctime>

CommandHandler::CommandHandler() {
    currentSession.active = false;
    currentSession.user = "";
    currentSession.mountId = "";
    currentSession.diskPath = "";
    currentSession.uid = -1;
    currentSession.gid = -1;
    currentSession.group = "";
}

CommandResult CommandHandler::processCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(stateMutex);
    
    CommandResult result;
    result.success = false;
    
    try {
        result = parser.parse(command);
    } catch (const std::exception& e) {
        result.success = false;
        result.message = "Error interno al analizar el comando: " + std::string(e.what());
        result.command = command;
        return result;
    }
    
    if (!result.success) {
        return result;
    }
    
    if (!validateCommandStructure(result)) {
        result.success = false;
        result.message = "Estructura del comando inválida";
        return result;
    }
    
    std::string cmd = result.data["command"];
    json params = result.data["parameters"];
    
    CommandResult processedResult;
    
    if (cmd == "mkdisk") {
        processedResult = processMkdisk(params);
    } else if (cmd == "rmdisk") {
        processedResult = processRmdisk(params);
    } else if (cmd == "fdisk") {
        processedResult = processFdisk(params);
    } else if (cmd == "mount") {
        processedResult = processMount(params);
    } else if (cmd == "mounted") {
        processedResult = processMounted(params);
    } else if (cmd == "mkfs") {
        processedResult = processMkfs(params);
    } else if (cmd == "login") {
        processedResult = processLogin(params);
    } else if (cmd == "logout") {
        processedResult = processLogout(params);
    } else if (cmd == "mkgrp") {
        processedResult = processMkgrp(params);
    } else if (cmd == "rmgrp") {
        processedResult = processRmgrp(params);
    } else if (cmd == "mkusr") {
        processedResult = processMkusr(params);
    } else if (cmd == "rmusr") {
        processedResult = processRmusr(params);
    } else if (cmd == "chgrp") {
        processedResult = processChgrp(params);
    } else if (cmd == "mkfile") {
        processedResult = processMkfile(params);
    } else if (cmd == "mkdir") {
        processedResult = processMkdir(params);
    } else if (cmd == "cat") {
        processedResult = processCat(params);
    } else if (cmd == "rep") {
        processedResult = processRep(params);
    } else if (cmd == "lsdisk") {
        processedResult = processLsdisk(params);
    } else if (cmd == "lsjson") {
        processedResult = processLsjson(params);
    } else if (cmd == "lsreports") {
        processedResult = processLsreports(params);
    } else if (cmd == "unmount") {
        processedResult = processUnmount(params);
    } else if (cmd == "remove") {
        processedResult = processRemove(params);
    } else if (cmd == "rename") {
        processedResult = processRename(params);
    } else if (cmd == "copy") {
        processedResult = processCopy(params);
    } else if (cmd == "move") {
        processedResult = processMove(params);
    } else if (cmd == "find") {
        processedResult = processFind(params);
    } else if (cmd == "chown") {
        processedResult = processChown(params);
    } else if (cmd == "loss") {
        processedResult = processLoss(params);
    } else if (cmd == "journaling") {
        processedResult = processJournaling(params);
    } else {
        processedResult.success = false;
        processedResult.message = "Comando no implementado: " + cmd;
    }
    
    processedResult.command = command;
    processedResult.tokens = result.tokens;
    
    json mergedData = processedResult.data;
    if (mergedData.empty()) {
        mergedData = json::object();
    }
    mergedData["_command"] = cmd;
    mergedData["_parameters"] = params;
    processedResult.data = mergedData;
    
    return processedResult;
}

json CommandHandler::getSessionStatus() {
    std::lock_guard<std::mutex> lock(stateMutex);
    
    json status;
    status["active"] = currentSession.active;
    status["user"] = currentSession.user;
    status["mountId"] = currentSession.mountId;
    status["diskPath"] = currentSession.diskPath;
    status["uid"] = currentSession.uid;
    status["gid"] = currentSession.gid;
    status["group"] = currentSession.group;
    
    json mounted = json::array();
    for (const auto& entry : mountedDisks) {
        json item;
        item["id"] = entry.first;
        item["disk"] = entry.second;
        mounted.push_back(item);
    }
    status["mounted"] = mounted;
    status["mountedCount"] = mountedDisks.size();
    
    return status;
}

CommandResult CommandHandler::loginFromGUI(const std::string& id, const std::string& user, const std::string& pass) {
    json params;
    params["id"] = id;
    params["user"] = user;
    params["pass"] = pass;
    return processLogin(params);
}

CommandResult CommandHandler::logoutFromGUI() {
    json params;
    return processLogout(params);
}

// ============================================================
// ✅ IMPLEMENTADO: Escribir en el journal
// 
// El journal está ubicado en:
//   partitionStart + sizeof(Superblock)
// 
// Cada entrada es un struct Journal (114 bytes):
//   int32_t j_count (4)
//   Information j_content (110)
// 
// El primer Journal (índice 0) tiene j_count = 1, 2, 3, ... (contador)
// Los siguientes Journals tienen j_count = 0 (disponibles)
// 
// Al escribir, se busca el primer Journal con j_count == 0
// ============================================================
void CommandHandler::writeJournal(const Superblock& sb, int partitionIndex,
                                   const std::string& operation, const std::string& path,
                                   const std::string& content) {
    try {
        // 1. Validar que sea EXT3
        if (sb.s_filesystem_type != 3) {
            return;  // Solo EXT3 tiene journal
        }
        
        // 2. Obtener info del disco
        if (currentSession.diskPath.empty()) return;
        std::string diskPath = currentSession.diskPath;
        
        // 3. Abrir disco
        std::fstream disk(diskPath, std::ios::in | std::ios::out | std::ios::binary);
        if (!disk.is_open()) return;
        
        // 4. Calcular la ubicación del journal
        // Necesitamos el MBR para saber dónde empieza la partición
        MBR mbr;
        disk.seekg(0, std::ios::beg);
        disk.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        
        int64_t partitionStart = mbr.mbr_partitions[partitionIndex].part_start;
        int64_t journalStart = partitionStart + sizeof(Superblock);
        
        // 5. Calcular cuántos journals caben
        // El journaling ocupa: 50 * numInodes bytes (constante del PDF)
        // Pero cada Journal real ocupa sizeof(Journal) bytes
        // 
        // Para simplificar, escribimos en el área de journaling
        // buscando el primer slot libre
        
        // Leer el primer journal para ver el contador
        Journal firstJournal;
        disk.seekg(journalStart, std::ios::beg);
        disk.read(reinterpret_cast<char*>(&firstJournal), sizeof(Journal));
        
        // El contador está en firstJournal.j_count
        int32_t currentCount = firstJournal.j_count;
        if (currentCount < 0) currentCount = 0;
        if (currentCount > 1000000) currentCount = 0;  // protección
        
        // 6. Buscar el primer Journal libre
        // Empezamos desde el Journal 1 (el 0 es el contador)
        int maxJournals = (50 * sb.s_inodes_count) / sizeof(Journal);
        if (maxJournals < 2) maxJournals = 2;
        if (maxJournals > 10000) maxJournals = 10000;  // límite de seguridad
        
        int freeSlot = -1;
        for (int i = 1; i < maxJournals; i++) {
            int64_t offset = journalStart + (int64_t)i * sizeof(Journal);
            
            // Verificar que no nos salgamos de la partición
            if (offset + sizeof(Journal) > partitionStart + mbr.mbr_partitions[partitionIndex].part_s) {
                break;
            }
            
            Journal j;
            disk.seekg(offset, std::ios::beg);
            disk.read(reinterpret_cast<char*>(&j), sizeof(Journal));
            
            // Un journal está libre si su j_count == 0
            if (j.j_count == 0) {
                freeSlot = i;
                break;
            }
        }
        
        if (freeSlot == -1) {
            // No hay espacio, sobreescribir el más antiguo (índice 1)
            freeSlot = 1;
        }
        
        // 7. Crear la entrada del journal
        Journal entry;
        memset(&entry, 0, sizeof(Journal));
        entry.j_count = currentCount + 1;  // contador incremental
        
        // Copiar operación (máx 9 chars + \0)
        strncpy(entry.j_content.i_operation, operation.c_str(), 9);
        
        // Copiar path (máx 31 chars + \0)
        strncpy(entry.j_content.i_path, path.c_str(), 31);
        
        // Copiar contenido (máx 63 chars + \0)
        strncpy(entry.j_content.i_content, content.c_str(), 63);
        
        // Fecha actual como float (timestamp)
        entry.j_content.i_date = (float)time(nullptr);
        
        // 8. Escribir la entrada
        int64_t writeOffset = journalStart + (int64_t)freeSlot * sizeof(Journal);
        disk.seekp(writeOffset, std::ios::beg);
        disk.write(reinterpret_cast<const char*>(&entry), sizeof(Journal));
        
        // 9. Actualizar el contador en el Journal 0
        firstJournal.j_count = currentCount + 1;
        disk.seekp(journalStart, std::ios::beg);
        disk.write(reinterpret_cast<const char*>(&firstJournal), sizeof(Journal));
        
        disk.close();
        
    } catch (const std::exception& e) {
        // Silenciar errores del journal para no romper el comando principal
        (void)e;
    }
}

bool CommandHandler::validateCommandStructure(const CommandResult& result) {
    if (!result.data.contains("parameters")) {
        return false;
    }
    if (!result.data["parameters"].is_object()) {
        return false;
    }
    return true;
}

bool CommandHandler::validateDiskExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool CommandHandler::validatePartitionName(const std::string& path, const std::string& name) {
    (void)path;
    (void)name;
    return true;
}

bool CommandHandler::validateMountId(const std::string& id) {
    return mountedDisks.find(id) != mountedDisks.end();
}

bool CommandHandler::isLoggedIn() {
    return currentSession.active;
}

bool CommandHandler::isRoot() {
    return currentSession.active && currentSession.user == "root";
}