#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

struct Token {
    std::string type;
    std::string value;
    int line;
    int column;
};

struct ErrorInfo {
    std::string type;      // "lexical", "syntactic", "semantic"
    std::string message;
    int line;
    int column;
};

struct CommandResult {
    bool success;
    std::string command;
    std::vector<ErrorInfo> errors;
    std::vector<Token> tokens;
    json data;  // Datos específicos del comando
    std::string message;
};

// Funciones de utilidad JSON
json tokenToJson(const Token& token);
json errorToJson(const ErrorInfo& error);
std::string resultToJson(const CommandResult& result);