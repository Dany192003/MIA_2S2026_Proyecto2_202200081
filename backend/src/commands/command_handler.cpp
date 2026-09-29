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
// Consultar estado de sesión
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
    
    // También exponer las particiones montadas
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