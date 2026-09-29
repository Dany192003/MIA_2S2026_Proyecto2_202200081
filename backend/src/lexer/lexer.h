#pragma once
#include <string>
#include <vector>
#include "../utils/json_utils.h"

class Lexer {
public:
    Lexer();
    std::vector<Token> tokenize(const std::string& input);
    bool hasErrors() const;
    std::vector<ErrorInfo> getErrors() const;
    
private:
    std::vector<Token> tokens;
    std::vector<ErrorInfo> errors;
    int currentLine;
    int currentColumn;
    int position;
    std::string source;
    
    void advance();
    char peek();
    bool isAtEnd();
    void skipWhitespace();
    Token makeToken(const std::string& type, const std::string& value);
    void addError(const std::string& message);
    
    // Métodos de reconocimiento
    void identifyCommand();
    void identifyParameter();
    void identifyValue();
    void identifyString();
    void identifyNumber();
};