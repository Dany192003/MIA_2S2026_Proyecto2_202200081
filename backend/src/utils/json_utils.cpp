#include "json_utils.h"

json tokenToJson(const Token& token) {
    json j;
    j["type"] = token.type;
    j["value"] = token.value;
    j["line"] = token.line;
    j["column"] = token.column;
    return j;
}

json errorToJson(const ErrorInfo& error) {
    json j;
    j["type"] = error.type;
    j["message"] = error.message;
    j["line"] = error.line;
    j["column"] = error.column;
    return j;
}

std::string resultToJson(const CommandResult& result) {
    json j;
    j["success"] = result.success;
    j["command"] = result.command;
    j["message"] = result.message;
    
    json tokensArray = json::array();
    for (const auto& token : result.tokens) {
        tokensArray.push_back(tokenToJson(token));
    }
    j["tokens"] = tokensArray;
    
    json errorsArray = json::array();
    for (const auto& error : result.errors) {
        errorsArray.push_back(errorToJson(error));
    }
    j["errors"] = errorsArray;
    
    if (!result.data.empty()) {
        j["data"] = result.data;
    }
    
    // ✅ BUG 2 CORREGIDO: Usar error_handler_t::replace para evitar excepciones
    // Reemplaza caracteres inválidos por el carácter de reemplazo Unicode U+FFFD
    try {
        return j.dump(4, ' ', false, json::error_handler_t::replace);
    } catch (const std::exception& e) {
        // Fallback seguro: devolver un JSON mínimo
        json error;
        error["success"] = false;
        error["message"] = "Error al serializar respuesta";
        error["error"] = e.what();
        return error.dump(4);
    }
}