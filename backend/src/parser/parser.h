#pragma once
#include <string>
#include <vector>
#include <map>
#include "../utils/json_utils.h"
#include "../lexer/lexer.h"

class Parser {
public:
    Parser();
    CommandResult parse(const std::string& input);
    
private:
    std::vector<Token> tokens;
    int currentToken;
    std::vector<ErrorInfo> errors;
    
    // ===== COMANDOS VÁLIDOS =====
    std::vector<std::string> validCommands = {
        "mkdisk", "rmdisk", "fdisk", "mount", "mounted",
        "mkfs", "login", "logout",
        "mkgrp", "rmgrp", "mkusr", "rmusr", "chgrp",
        "mkfile", "mkdir", "cat", "rep", "lsdisk", "lsjson", "lsreports"
    };
    
    // ===== PARÁMETROS ESPERADOS POR COMANDO =====
    std::map<std::string, std::vector<std::string>> commandParams = {
        {"mkdisk", {"size", "fit", "unit", "path"}},
        {"rmdisk", {"path"}},
        {"fdisk", {"size", "unit", "path", "type", "fit", "name"}},
        {"mount", {"path", "name"}},
        {"mounted", {}},
        {"mkfs", {"id", "type"}},
        {"login", {"user", "pass", "id"}},
        {"logout", {}},
        {"mkgrp", {"name"}},
        {"rmgrp", {"name"}},
        {"mkusr", {"user", "pass", "grp"}},
        {"rmusr", {"user"}},
        {"chgrp", {"user", "grp"}},
        {"mkfile", {"path", "r", "size", "cont"}},
        {"mkdir", {"path", "p"}},
        {"cat", {}},  // Parámetros dinámicos: file1, file2, ...
        {"rep", {"name", "path", "id", "path_file_ls"}},
        {"lsdisk", {}},
        {"lsjson", {"path", "id"}},
        {"lsreports", {}}
    };
    
    // ===== PARÁMETROS OBLIGATORIOS POR COMANDO =====
    std::map<std::string, std::vector<std::string>> requiredParams = {
        {"mkdisk", {"size", "path"}},
        {"rmdisk", {"path"}},
        {"fdisk", {"size", "path", "name"}},
        {"mount", {"path", "name"}},
        {"mounted", {}},
        {"mkfs", {"id"}},
        {"login", {"user", "pass", "id"}},
        {"logout", {}},
        {"mkgrp", {"name"}},
        {"rmgrp", {"name"}},
        {"mkusr", {"user", "pass", "grp"}},
        {"rmusr", {"user"}},
        {"chgrp", {"user", "grp"}},
        {"mkfile", {"path"}},
        {"mkdir", {"path"}},
        {"cat", {}},
        {"rep", {"name", "path", "id"}},
        {"lsjson", {"path", "id"}}
    };
    
    // ===== MÉTODOS DE CONTROL =====
    bool match(const std::string& type);
    bool check(const std::string& type);
    Token advance();
    Token peekToken();
    bool isAtEnd();
    void addError(const std::string& message);
    void synchronize();
    
    // ===== MÉTODOS DE PARSING =====
    void parseCommand(CommandResult& result);
    void parseParameters(CommandResult& result, const std::string& command);
    std::map<std::string, std::string> parseParameterList();
    bool validateParameterValue(const std::string& param, const std::string& value, const std::string& command);
    std::string normalizeValue(const std::string& value);
    
    // ===== VALIDACIONES ESPECÍFICAS =====
    bool validateSize(const std::string& value);
    bool validateUnit(const std::string& value, const std::string& command);
    bool validateFit(const std::string& value);
    
    // validateType recibe el comando
    bool validateType(const std::string& value, const std::string& command);
    
    bool validateName(const std::string& value);
    bool validatePath(const std::string& value);
};