#include "parser.h"
#include <algorithm>
#include <cctype>

Parser::Parser() : currentToken(0) {}

CommandResult Parser::parse(const std::string& input) {
    CommandResult result;
    result.command = input;
    result.success = false;
    errors.clear();
    
    // Tokenizar primero
    Lexer lexer;
    tokens = lexer.tokenize(input);
    
    // Verificar errores léxicos
    if (lexer.hasErrors()) {
        result.errors = lexer.getErrors();
        result.message = "Errores léxicos encontrados";
        return result;
    }
    
    currentToken = 0;
    
    try {
        // Verificar que hay tokens
        if (tokens.empty()) {
            addError("Comando vacío");
            result.errors = errors;
            result.message = "No se ingresó ningún comando";
            return result;
        }
        
        // Parsear el comando
        parseCommand(result);
        
        // Verificar que no haya tokens sobrantes
        if (!isAtEnd()) {
            addError("Tokens no esperados al final del comando");
        }
        
        if (errors.empty()) {
            result.success = true;
            result.message = "Comando analizado correctamente";
        } else {
            result.message = "Errores sintácticos encontrados";
        }
        
    } catch (const std::exception& e) {
        addError(std::string("Error durante el parseo: ") + e.what());
    }
    
    result.errors = errors;
    result.tokens = tokens;
    return result;
}

bool Parser::match(const std::string& type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(const std::string& type) {
    if (isAtEnd()) return false;
    return tokens[currentToken].type == type;
}

Token Parser::advance() {
    if (!isAtEnd()) currentToken++;
    return tokens[currentToken - 1];
}

Token Parser::peekToken() {
    if (isAtEnd()) return {"", "", -1, -1};
    return tokens[currentToken];
}

bool Parser::isAtEnd() {
    return currentToken >= (int)tokens.size();
}

void Parser::addError(const std::string& message) {
    if (isAtEnd()) {
        errors.push_back({"syntactic", message, 0, 0});
    } else {
        Token token = peekToken();
        errors.push_back({"syntactic", message, token.line, token.column});
    }
}

void Parser::synchronize() {
    while (!isAtEnd()) {
        Token token = peekToken();
        if (token.type == "COMMAND" || token.type == "PARAMETER") {
            return;
        }
        advance();
    }
}

void Parser::parseCommand(CommandResult& result) {
    // Verificar que el primer token sea un comando
    if (!check("COMMAND")) {
        addError("Se esperaba un comando");
        return;
    }
    
    Token commandToken = advance();
    std::string command = commandToken.value;
    
    // Verificar que el comando sea válido
    if (std::find(validCommands.begin(), validCommands.end(), command) == validCommands.end()) {
        addError("Comando no reconocido: '" + command + "'");
        return;
    }
    
    // Guardar el comando en el resultado
    result.data["command"] = command;
    
    // Parsear parámetros
    parseParameters(result, command);
}

void Parser::parseParameters(CommandResult& result, const std::string& command) {
    std::map<std::string, std::string> params;
    std::vector<std::string> foundParams;
    
    // Obtener parámetros esperados
    auto it = commandParams.find(command);
    if (it == commandParams.end()) {
        addError("No hay definición de parámetros para el comando: " + command);
        return;
    }
    
    std::vector<std::string> expectedParams = it->second;
    
    // Parsear todos los parámetros
    while (!isAtEnd() && (check("PARAMETER") || check("FLAG"))) {
        Token token = peekToken();
        
        if (token.type == "FLAG") {
            // Parámetro sin valor (como -r, -p)
            advance();
            std::string paramName = token.value;
            params[paramName] = "true";
            foundParams.push_back(paramName);
            continue;
        }
        
        if (token.type == "PARAMETER") {
            advance(); // Consumir PARAMETER
            std::string paramName = token.value;
            
            // Permitir file1, file2, ... para cat
            bool isValidParam = false;
            
            if (command == "cat" && paramName.find("file") == 0) {
                isValidParam = true;
            }
            else if (std::find(expectedParams.begin(), expectedParams.end(), paramName) != expectedParams.end()) {
                isValidParam = true;
            }
            
            if (!isValidParam) {
                addError("Parámetro no esperado: '" + paramName + "' para el comando " + command);
                continue;
            }
            
            // Esperar '='
            if (!check("EQUALS")) {
                addError("Se esperaba '=' después del parámetro: " + paramName);
                continue;
            }
            advance(); // Consumir EQUALS
            
            // Obtener valor
            if (check("VALUE") || check("NUMBER") || check("STRING")) {
                Token valueToken = advance();
                std::string value = valueToken.value;
                
                // Validar valor según el parámetro
                if (!validateParameterValue(paramName, value, command)) {
                    addError("Valor inválido para el parámetro '" + paramName + "': " + value);
                }
                
                params[paramName] = value;
                foundParams.push_back(paramName);
            } else {
                addError("Se esperaba un valor para el parámetro: " + paramName);
            }
        }
    }
    
    // Verificar parámetros obligatorios
    auto reqIt = requiredParams.find(command);
    if (reqIt != requiredParams.end()) {
        std::vector<std::string> required = reqIt->second;
        for (const std::string& req : required) {
            if (std::find(foundParams.begin(), foundParams.end(), req) == foundParams.end()) {
                addError("Parámetro obligatorio faltante: '" + req + "'");
            }
        }
    }
    
    // Guardar parámetros en el resultado
    result.data["parameters"] = json(params);
}

std::map<std::string, std::string> Parser::parseParameterList() {
    std::map<std::string, std::string> params;
    return params;
}

bool Parser::validateParameterValue(const std::string& param, const std::string& value, const std::string& command) {
    // Para cat, no validamos los valores de file1, file2, ...
    if (command == "cat" && param.find("file") == 0) {
        return true;
    }
    
    if (param == "size") {
        return validateSize(value);
    }
    else if (param == "unit") {
        return validateUnit(value, command);
    }
    else if (param == "fit") {
        return validateFit(value);
    }
    else if (param == "type") {
       
        return validateType(value, command);
    }
    else if (param == "name") {
        return validateName(value);
    }
    else if (param == "path") {
        return validatePath(value);
    }
    else if (param == "user" || param == "pass" || param == "grp") {
        return value.length() <= 10;
    }
    else if (param == "r" || param == "p" || param == "f") {
        return true;
    }
    else if (param == "path_file_ls") {
        return validatePath(value);
    }
    else if (param == "id") {
        return !value.empty();
    }
    else if (param == "cont") {
        return validatePath(value);
    }
    return true;
}

std::string Parser::normalizeValue(const std::string& value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

bool Parser::validateSize(const std::string& value) {
    if (value.empty()) return false;
    
    try {
        int size = std::stoi(value);
        return size > 0;
    } catch (const std::exception& e) {
        return false;
    }
}

bool Parser::validateUnit(const std::string& value, const std::string& command) {
    std::string unit = normalizeValue(value);
    
    if (command == "mkdisk") {
        return unit == "k" || unit == "m";
    } else if (command == "fdisk") {
        return unit == "b" || unit == "k" || unit == "m";
    }
    
    return false;
}

bool Parser::validateFit(const std::string& value) {
    std::string fit = normalizeValue(value);
    return fit == "bf" || fit == "ff" || fit == "wf";
}

// validateType valida según el comando
bool Parser::validateType(const std::string& value, const std::string& command) {
    std::string type = normalizeValue(value);
    
    if (command == "fdisk") {
        // Solo P, E, L para fdisk
        return type == "p" || type == "e" || type == "l";
    }
    if (command == "mkfs") {
        // Solo "full" para mkfs
        return type == "full";
    }
    return false;
}

bool Parser::validateName(const std::string& value) {
    if (value.empty()) return false;
    
    for (char c : value) {
        if (!std::isalnum(c) && c != '_' && c != '-' && c != ' ') {
            return false;
        }
    }
    
    return true;
}

bool Parser::validatePath(const std::string& value) {
    if (value.empty()) return false;
    
    // Aceptar rutas relativas y absolutas
    // No permitir caracteres peligrosos
    for (char c : value) {
        if (c == '<' || c == '>' || c == '|' || c == '&' || c == ';' || c == '`') {
            return false;
        }
    }
    
    return true;
}