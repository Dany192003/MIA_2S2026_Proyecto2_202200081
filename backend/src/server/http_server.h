#pragma once
#include <crow.h>
#include "../commands/command_handler.h"

class HttpServer {
public:
    HttpServer();
    void start(int port = 8080);
    void stop();
    
private:
    crow::SimpleApp app;
    CommandHandler commandHandler;
    
    void setupRoutes();
};