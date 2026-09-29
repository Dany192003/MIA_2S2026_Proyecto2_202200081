#include "command_handler.h"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <cstdlib>

namespace fs = std::filesystem;

// Ruta configurable con fallback absoluto
static std::string getReportsDir() {
    const char* env = std::getenv("EXT2_REPORTS_DIR");
    if (env && strlen(env) > 0) {
        return std::string(env);
    }
    return "/home/Edwin/Desktop/MIA_2S2026_Proyecto1_202200081/reports/";
}

CommandResult CommandHandler::processLsreports(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        std::string reportsDir = getReportsDir();
        
        json reportList = json::array();
        
        if (!fs::exists(reportsDir)) {
            result.message = "La carpeta reports/ no existe: " + reportsDir;
            result.data["reports"] = json::array();
            result.data["total"] = 0;
            result.success = true;
            return result;
        }
        
        for (const auto& entry : fs::directory_iterator(reportsDir)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                std::string ext = entry.path().extension().string();
                
                if (ext == ".png" || ext == ".txt" || ext == ".dot" || ext == ".jpg") {
                    json item;
                    item["name"] = filename;
                    item["path"] = entry.path().string();
                    item["size"] = (int64_t)entry.file_size();
                    item["extension"] = ext;
                    
                    if (ext == ".png" || ext == ".jpg") item["type"] = "image";
                    else if (ext == ".txt") item["type"] = "text";
                    else if (ext == ".dot") item["type"] = "dot";
                    
                    reportList.push_back(item);
                }
            }
        }
        
        result.data["reports"] = reportList;
        result.data["total"] = reportList.size();
        result.message = "Reportes encontrados: " + std::to_string(reportList.size());
        result.success = true;
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar LSREPORTS: " + std::string(e.what());
    }
    
    return result;
}