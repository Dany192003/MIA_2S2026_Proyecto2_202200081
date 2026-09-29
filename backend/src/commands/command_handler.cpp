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
    
    // ===== COMANDOS DEL PROYECTO 1 =====
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
    }
    // ===== COMANDOS NUEVOS DEL PROYECTO 2 =====
    else if (cmd == "unmount") {
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

// ============================================================
// Endpoint para consultar sesión activa
// ============================================================
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

// ============================================================
// ✅ NUEVO: Login desde GUI
// ============================================================
CommandResult CommandHandler::loginFromGUI(const std::string& id, const std::string& user, const std::string& pass) {
    json params;
    params["id"] = id;
    params["user"] = user;
    params["pass"] = pass;
    return processLogin(params);
}

// ============================================================
// ✅ NUEVO: Logout desde GUI
// ============================================================
CommandResult CommandHandler::logoutFromGUI() {
    json params;
    return processLogout(params);
}

// ============================================================
// ✅ NUEVO: Escribir en el journal
// ============================================================
void CommandHandler::writeJournal(const Superblock& sb, int partitionIndex,
                                   const std::string& operation, const std::string& path,
                                   const std::string& content) {
    // TODO: Implementar en el siguiente paso
    (void)sb;
    (void)partitionIndex;
    (void)operation;
    (void)path;
    (void)content;
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