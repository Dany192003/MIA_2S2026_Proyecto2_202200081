#include "http_server.h"
#include "../utils/json_utils.h"
#include <iostream>

HttpServer::HttpServer() {
    setupRoutes();
}

void HttpServer::setupRoutes() {
    // ============================================================
    // Ruta POST /analyze
    // ============================================================
    CROW_ROUTE(app, "/analyze").methods(crow::HTTPMethod::POST, crow::HTTPMethod::Options)
    ([this](const crow::request& req) {
        crow::response res;
        
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type");
        
        if (req.method == crow::HTTPMethod::Options) {
            res.code = 200;
            res.body = "";
            return res;
        }
        
        try {
            auto body = crow::json::load(req.body);
            if (!body) {
                res.code = 400;
                res.body = "{\"error\":\"Invalid JSON\"}";
                res.add_header("Content-Type", "application/json");
                return res;
            }
            
            std::string command = body["command"].s();
            CommandResult result = commandHandler.processCommand(command);
            std::string response = resultToJson(result);
            
            res.code = 200;
            res.body = response;
            res.add_header("Content-Type", "application/json");
            return res;
        } catch (const std::exception& e) {
            res.code = 500;
            res.body = "{\"error\":\"" + std::string(e.what()) + "\"}";
            res.add_header("Content-Type", "application/json");
            return res;
        }
    });

    // ============================================================
    //GET /session/status
    // ============================================================
    CROW_ROUTE(app, "/session/status").methods(crow::HTTPMethod::GET, crow::HTTPMethod::Options)
    ([this](const crow::request& req) {
        crow::response res;
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Access-Control-Allow-Methods", "GET, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type");
        res.add_header("Content-Type", "application/json");
        
        if (req.method == crow::HTTPMethod::Options) {
            res.code = 200;
            res.body = "";
            return res;
        }
        
        try {
            json status = commandHandler.getSessionStatus();
            res.code = 200;
            res.body = status.dump();
            return res;
        } catch (const std::exception& e) {
            res.code = 500;
            json err;
            err["error"] = e.what();
            res.body = err.dump();
            return res;
        }
    });

    // ============================================================
    // Ruta GET /status
    // ============================================================
    CROW_ROUTE(app, "/status").methods(crow::HTTPMethod::GET)
    ([this]() {
        crow::response res;
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Content-Type", "application/json");
        
        json status;
        status["status"] = "ok";
        status["message"] = "Servidor de análisis EXT2 funcionando";
        
        res.code = 200;
        res.body = status.dump();
        return res;
    });

    // ============================================================
    // Ruta GET /health
    // ============================================================
    CROW_ROUTE(app, "/health").methods(crow::HTTPMethod::GET)
    ([]() {
        crow::response res;
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Content-Type", "application/json");
        res.code = 200;
        res.body = "{\"status\":\"ok\"}";
        return res;
    });

    // ============================================================
    // Ruta GET /commands
    // ============================================================
    CROW_ROUTE(app, "/commands").methods(crow::HTTPMethod::GET)
    ([]() {
        crow::response res;
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Content-Type", "application/json");
        
        json commands = {
            {"commands", {
                "mkdisk", "rmdisk", "fdisk", "mount", "mounted",
                "mkfs", "login", "logout",
                "mkgrp", "rmgrp", "mkusr", "rmusr", "chgrp",
                "mkfile", "mkdir", "cat", "rep"
            }}
        };
        
        res.code = 200;
        res.body = commands.dump();
        return res;
    });
}

void HttpServer::start(int port) {
    std::cout << "========================================" << std::endl;
    std::cout << "   Servidor EXT2 Analyzer" << std::endl;
    std::cout << "   Puerto: " << port << std::endl;
    std::cout << "   Endpoints:" << std::endl;
    std::cout << "   - POST /analyze" << std::endl;
    std::cout << "   - GET  /session/status" << std::endl;
    std::cout << "   - GET  /status" << std::endl;
    std::cout << "   - GET  /health" << std::endl;
    std::cout << "   - GET  /commands" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Servidor iniciado en http://localhost:" << port << std::endl;
    std::cout << "Presiona Ctrl+C para detener" << std::endl;
    std::cout << "========================================" << std::endl;
    
    app.port(port).multithreaded().run();
}

void HttpServer::stop() {
    std::cout << "Deteniendo servidor..." << std::endl;
}