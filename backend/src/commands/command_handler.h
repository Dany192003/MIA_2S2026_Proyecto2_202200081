#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include "../utils/json_utils.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"

#include "../structures/mbr.h"
#include "../structures/ebr.h"
#include "../structures/superblock.h"
#include "../structures/inode.h"
#include "../structures/block.h"

class CommandHandler {
public:
    CommandHandler();
    CommandResult processCommand(const std::string& command);
    
    //Endpoint para consultar sesión activa
    json getSessionStatus();
    
private:
    Lexer lexer;
    Parser parser;
    
    // ===== ESTRUCTURAS DE DATOS EN MEMORIA =====
    
    // Discos montados (id -> ruta del disco)
    std::map<std::string, std::string> mountedDisks;
    
    // Sesión activa
    struct Session {
        bool active;
        std::string user;
        std::string mountId;
        std::string diskPath;
        int uid;
        int gid;
        std::string group;
    };
    Session currentSession;
    
    //UN SOLO MUTEX PARA TODO EL ESTADO
    std::mutex stateMutex;
    
    // ===== MÉTODOS DE VALIDACIÓN =====
    bool validateCommandStructure(const CommandResult& result);
    bool validateDiskExists(const std::string& path);
    bool validatePartitionName(const std::string& path, const std::string& name);
    bool validateMountId(const std::string& id);
    bool isLoggedIn();
    bool isRoot();
    
    // ===== COMANDOS =====
    CommandResult processMkdisk(const json& params);
    CommandResult processRmdisk(const json& params);
    CommandResult processFdisk(const json& params);
    CommandResult processMount(const json& params);
    CommandResult processMounted(const json& params);
    CommandResult processMkfs(const json& params);
    CommandResult processLogin(const json& params);
    CommandResult processLogout(const json& params);
    CommandResult processMkgrp(const json& params);
    CommandResult processRmgrp(const json& params);
    CommandResult processMkusr(const json& params);
    CommandResult processRmusr(const json& params);
    CommandResult processChgrp(const json& params);
    CommandResult processMkfile(const json& params);
    CommandResult processMkdir(const json& params);
    CommandResult processCat(const json& params);
    CommandResult processRep(const json& params);
    CommandResult processLsdisk(const json& params);
    CommandResult processLsjson(const json& params);
    CommandResult processLsreports(const json& params);
};

#endif