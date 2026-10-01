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
    // ✅ NUEVO: POST /login — Login desde GUI
    // ============================================================
    CROW_ROUTE(app, "/login").methods(crow::HTTPMethod::POST, crow::HTTPMethod::Options)
    ([this](const crow::request& req) {
        crow::response res;
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type");
        res.add_header("Content-Type", "application/json");
        
        if (req.method == crow::HTTPMethod::Options) {
            res.code = 200;
            res.body = "";
            return res;
        }
        
        try {
            auto body = crow::json::load(req.body);
            if (!body) {
                res.code = 400;
                res.body = "{\"success\":false,\"message\":\"Invalid JSON\"}";
                return res;
            }
            
            std::string id = body["id"].s();
            std::string user = body["user"].s();
            std::string pass = body["pass"].s();
            
            CommandResult result = commandHandler.loginFromGUI(id, user, pass);
            std::string response = resultToJson(result);
            
            res.code = result.success ? 200 : 401;
            res.body = response;
            return res;
        } catch (const std::exception& e) {
            res.code = 500;
            json err;
            err["success"] = false;
            err["message"] = std::string("Error: ") + e.what();
            res.body = err.dump();
            return res;
        }
    });

    // ============================================================
    // ✅ NUEVO: POST /logout — Logout desde GUI
    // ============================================================
    CROW_ROUTE(app, "/logout").methods(crow::HTTPMethod::POST, crow::HTTPMethod::Options)
    ([this](const crow::request& req) {
        crow::response res;
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type");
        res.add_header("Content-Type", "application/json");
        
        if (req.method == crow::HTTPMethod::Options) {
            res.code = 200;
            res.body = "";
            return res;
        }
        
        try {
            CommandResult result = commandHandler.logoutFromGUI();
            std::string response = resultToJson(result);
            
            res.code = 200;
            res.body = response;
            return res;
        } catch (const std::exception& e) {
            res.code = 500;
            json err;
            err["success"] = false;
            err["message"] = std::string("Error: ") + e.what();
            res.body = err.dump();
            return res;
        }
    });

    // ============================================================
    // ✅ NUEVO: GET /disks — Lista de discos (equivalente a lsdisk)
    // ============================================================
    CROW_ROUTE(app, "/disks").methods(crow::HTTPMethod::GET, crow::HTTPMethod::Options)
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
            // Ejecutar el comando lsdisk internamente
            CommandResult result = commandHandler.processCommand("lsdisk");
            
            // Devolver directamente el contenido de data
            if (result.success) {
                res.code = 200;
                res.body = result.data.dump();
            } else {
                res.code = 500;
                json err;
                err["success"] = false;
                err["message"] = result.message;
                res.body = err.dump();
            }
            return res;
        } catch (const std::exception& e) {
            res.code = 500;
            json err;
            err["success"] = false;
            err["message"] = std::string("Error: ") + e.what();
            res.body = err.dump();
            return res;
        }
    });

    // ============================================================
    // GET /session/status
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
                "mkfile", "mkdir", "cat", "rep",
                "unmount", "remove", "rename", "copy",
                "move", "find", "chown", "loss", "journaling"
            }}
        };
        
        res.code = 200;
        res.body = commands.dump();
        return res;
    });
}

void HttpServer::start(int port) {
    std::cout << "========================================" << std::endl;
    std::cout << "   Servidor EXT2/EXT3 Analyzer" << std::endl;
    std::cout << "   Puerto: " << port << std::endl;
    std::cout << "   Endpoints:" << std::endl;
    std::cout << "   - POST /analyze" << std::endl;
    std::cout << "   - POST /login" << std::endl;
    std::cout << "   - POST /logout" << std::endl;
    std::cout << "   - GET  /disks" << std::endl;
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