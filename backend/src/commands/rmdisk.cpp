#include "command_handler.h"
#include <unistd.h>

CommandResult CommandHandler::processRmdisk(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        // 1. Obtener parámetros
        std::string path = params["path"];
        
        // 2. Validar que la ruta no esté vacía
        if (path.empty()) {
            result.message = "Error: La ruta no puede estar vacía";
            return result;
        }
        
        // 3. Verificar si el archivo existe
        if (!validateDiskExists(path)) {
            result.message = "Error: El disco no existe en la ruta: " + path;
            return result;
        }
        
        // 4. Verificar que el disco no esté montado
        for (const auto& mount : mountedDisks) {
            if (mount.second == path) {
                result.message = "Error: El disco está montado, no se puede eliminar";
                return result;
            }
        }
        
        // 5. Eliminar el archivo
        if (unlink(path.c_str()) != 0) {
            result.message = "Error: No se pudo eliminar el archivo: " + path;
            return result;
        }
        
        // 6. Éxito
        result.success = true;
        result.message = "Disco eliminado exitosamente: " + path;
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar RMDISK: " + std::string(e.what());
    }
    
    return result;
}