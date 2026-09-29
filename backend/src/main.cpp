#include "server/http_server.h"
#include <iostream>
#include <csignal>
#include <cstdlib>

volatile sig_atomic_t running = 1;

void signalHandler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        std::cout << "\nCerrando servidor..." << std::endl;
        running = 0;
    }
}

void printBanner() {
    std::cout << "========================================" << std::endl;
    std::cout << "   EXT2 Analyzer - 2S 2026" << std::endl;
    std::cout << "   Servidor en http://localhost" << std::endl;
    std::cout << "========================================" << std::endl;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    printBanner();
    
    try {
        int port = 8080;
        if (argc > 1) {
            port = std::stoi(argv[1]);
            if (port < 1 || port > 65535) {
                std::cerr << "Error: Puerto inválido" << std::endl;
                return 1;
            }
        }
        
        std::cout << "Iniciando servidor en puerto " << port << "..." << std::endl;
        std::cout << std::endl;
        
        HttpServer server;
        server.start(port);
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Servidor terminado." << std::endl;
    return 0;
}