#include "command_handler.h"
#include <fstream>
#include <random>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

// Función auxiliar para crear carpetas recursivamente
static bool createDirectories(const std::string& path) {
    size_t pos = 0;
    std::string currentPath;
    
    // Si la ruta es absoluta, empezar con "/"
    if (path[0] == '/') {
        currentPath = "/";
        pos = 1;
    }
    
    while (pos < path.length()) {
        size_t nextSlash = path.find('/', pos);
        if (nextSlash == std::string::npos) {
            break;
        }
        
        std::string dir = path.substr(0, nextSlash);
        if (dir.length() > 0 && dir != "/") {
            if (mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
                return false;
            }
        }
        pos = nextSlash + 1;
    }
    return true;
}

// Función para generar número random
static int generateRandomSignature() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 2147483647);
    return dis(gen);
}

CommandResult CommandHandler::processMkdisk(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        // 1. Obtener parámetros
        std::string path = params["path"];
        int size = std::stoi(std::string(params["size"]));
        std::string unit = params.contains("unit") ? std::string(params["unit"]) : "m";
        std::string fit = params.contains("fit") ? std::string(params["fit"]) : "ff";
        
        // Convertir a minúsculas
        std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);
        std::transform(fit.begin(), fit.end(), fit.begin(), ::tolower);
        
        // 2. Validar tamaño (debe ser > 0)
        if (size <= 0) {
            result.message = "Error: El tamaño debe ser mayor que cero";
            return result;
        }
        
        // 3. Convertir tamaño a bytes
        int64_t sizeInBytes;
        if (unit == "k") {
            sizeInBytes = (int64_t)size * 1024;
        } else if (unit == "m") {
            sizeInBytes = (int64_t)size * 1024 * 1024;
        } else {
            result.message = "Error: Unidad inválida. Use K o M";
            return result;
        }
        
        // 4. Validar fit
        char fitChar;
        if (fit == "bf") fitChar = 'B';
        else if (fit == "ff") fitChar = 'F';
        else if (fit == "wf") fitChar = 'W';
        else {
            result.message = "Error: Ajuste inválido. Use BF, FF o WF";
            return result;
        }
        
        // 5. Crear carpetas si no existen
        size_t lastSlash = path.find_last_of('/');
        if (lastSlash != std::string::npos) {
            std::string dirPath = path.substr(0, lastSlash);
            if (!createDirectories(dirPath)) {
                result.message = "Error: No se pudieron crear las carpetas para: " + path;
                return result;
            }
        }
        
        // 6. Verificar si el archivo ya existe
        std::ifstream checkFile(path);
        if (checkFile.good()) {
            result.message = "Error: El disco ya existe en la ruta: " + path;
            return result;
        }
        checkFile.close();
        
        // 7. Crear el archivo del disco
        std::ofstream diskFile(path, std::ios::binary);
        if (!diskFile.is_open()) {
            result.message = "Error: No se pudo crear el archivo: " + path;
            return result;
        }
        
        // 8. Escribir ceros binarios (usando buffer de 1024 bytes para eficiencia)
        const int BUFFER_SIZE = 1024;
        char buffer[BUFFER_SIZE] = {0};
        int64_t bytesWritten = 0;
        
        while (bytesWritten < sizeInBytes) {
            int64_t chunkSize = std::min((int64_t)BUFFER_SIZE, sizeInBytes - bytesWritten);
            diskFile.write(buffer, chunkSize);
            bytesWritten += chunkSize;
        }
        
        diskFile.close();
        
        // 9. Escribir el MBR
        std::fstream disk(path, std::ios::in | std::ios::out | std::ios::binary);
        if (!disk.is_open()) {
            result.message = "Error: No se pudo abrir el disco para escribir MBR";
            return result;
        }
        
        MBR mbr;
        memset(&mbr, 0, sizeof(MBR));
        
        mbr.mbr_tamano = sizeInBytes;
        mbr.mbr_fecha_creacion = time(nullptr);
        mbr.mbr_dsk_signature = generateRandomSignature();
        mbr.dsk_fit = fitChar;
        
        // Inicializar particiones
        for (int i = 0; i < 4; i++) {
            mbr.mbr_partitions[i].part_status = '0';
            mbr.mbr_partitions[i].part_type = 'P';
            mbr.mbr_partitions[i].part_fit = fitChar;
            mbr.mbr_partitions[i].part_start = -1;
            mbr.mbr_partitions[i].part_s = 0;
            memset(mbr.mbr_partitions[i].part_name, 0, 16);
            mbr.mbr_partitions[i].part_correlative = -1;
            memset(mbr.mbr_partitions[i].part_id, 0, 4);
        }
        
        // Escribir MBR al inicio del archivo
        disk.seekp(0, std::ios::beg);
        disk.write(reinterpret_cast<const char*>(&mbr), sizeof(MBR));
        disk.close();
        
        // 10. Éxito
        result.success = true;
        result.message = "Disco creado exitosamente: " + path + " (Tamaño: " + std::to_string(size) + unit + ")";
        result.data["disk"] = {
            {"path", path},
            {"size", size},
            {"unit", unit},
            {"fit", fit},
            {"bytes", sizeInBytes}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar MKDISK: " + std::string(e.what());
    }
    
    return result;
}