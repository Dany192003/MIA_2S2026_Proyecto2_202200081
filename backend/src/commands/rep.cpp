#include "command_handler.h"
#include "reports/Report.h"
#include "../utils/ext2_utils.h"
#include <cstring>
#include <cstdlib>

// ✅ NUEVO: Resuelve ruta de reporte. Si es relativa, la combina con EXT2_REPORTS_DIR
static std::string resolveReportPath(const std::string& inputPath) {
    if (inputPath.empty()) return inputPath;
    
    // Si ya es absoluta, usar tal cual
    if (inputPath[0] == '/') return inputPath;
    
    // Ruta relativa → resolver contra EXT2_REPORTS_DIR
    std::string baseDir = Ext2Utils::getReportsDir();
    return baseDir + inputPath;
}

CommandResult CommandHandler::processRep(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        // Verificar sesión activa (según PDF)
        if (!isLoggedIn()) {
            result.message = "Error: No hay sesión activa. Use LOGIN primero.";
            return result;
        }
        
        std::string name = params["name"];
        std::string rawPath = params["path"];
        // ✅ NUEVO: Resolver ruta relativa contra EXT2_REPORTS_DIR
        std::string path = resolveReportPath(rawPath);
        
        std::string id = params["id"];
        std::string path_file = params.contains("path_file_ls") ? std::string(params["path_file_ls"]) : "";
        
        if (mountedDisks.find(id) == mountedDisks.end()) {
            result.message = "Error: El ID de montaje no existe: " + id;
            return result;
        }
        
        std::string diskPath = mountedDisks[id];
        
        std::fstream disk(diskPath, std::ios::in | std::ios::binary);
        if (!disk.is_open()) {
            result.message = "Error: No se pudo abrir el disco: " + diskPath;
            return result;
        }
        
        MBR mbr;
        disk.seekg(0, std::ios::beg);
        disk.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        disk.close();
        
        int partitionIndex = -1;
        for (int i = 0; i < 4; i++) {
            char partId[5] = {0};
            memcpy(partId, mbr.mbr_partitions[i].part_id, 4);
            partId[4] = '\0';
            std::string partIdStr(partId);
            partIdStr = partIdStr.c_str();
            
            if (partIdStr == id && mbr.mbr_partitions[i].part_s > 0) {
                partitionIndex = i;
                break;
            }
        }
        
        if (partitionIndex == -1) {
            result.message = "Error: No se encontró la partición para el ID: " + id;
            return result;
        }
        
        std::string errMsg;
        bool success = false;
        
        if (name == "mbr") {
            success = Reports::ReportMBR(mbr, path, diskPath, errMsg);
        } else if (name == "disk") {
            success = Reports::ReportDISK(mbr, path, diskPath, errMsg);
        } else if (name == "sb") {
            success = Reports::ReportSB(diskPath, mbr, partitionIndex, path, errMsg);
        } else if (name == "inode") {
            success = Reports::ReportINODE(diskPath, mbr, partitionIndex, path, errMsg);
        } else if (name == "block") {
            success = Reports::ReportBLOCK(diskPath, mbr, partitionIndex, path, errMsg);
        } else if (name == "bm_inode") {
            success = Reports::ReportBMInode(diskPath, mbr, partitionIndex, path, errMsg);
        } else if (name == "bm_block") {
            success = Reports::ReportBMBlock(diskPath, mbr, partitionIndex, path, errMsg);
        } else if (name == "tree") {
            success = Reports::ReportTREE(diskPath, mbr, partitionIndex, path, errMsg);
        } else if (name == "file") {
            if (path_file.empty()) {
                result.message = "Error: Se requiere path_file_ls para el reporte file";
                return result;
            }
            success = Reports::ReportFILE(diskPath, mbr, partitionIndex, path, path_file, errMsg);
        } else if (name == "ls") {
            if (path_file.empty()) {
                result.message = "Error: Se requiere path_file_ls para el reporte ls";
                return result;
            }
            success = Reports::ReportLS(diskPath, mbr, partitionIndex, path, path_file, errMsg);
        } else {
            result.message = "Error: Tipo de reporte inválido: " + name;
            return result;
        }
        
        if (!success) {
            result.message = "Error al generar reporte: " + errMsg;
            return result;
        }
        
        result.success = true;
        result.message = "Reporte generado exitosamente: " + path;
        result.data["report"] = {
            {"name", name},
            {"path", path},
            {"id", id}
        };
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar REP: " + std::string(e.what());
    }
    
    return result;
}