#include "command_handler.h"

CommandResult CommandHandler::processMounted(const json& params) {
    CommandResult result;
    result.success = false;
    
    try {
        // 1. Crear un array JSON para las particiones montadas
        json mountedList = json::array();
        std::string ids = "";
        
        // 2. Recorrer las particiones montadas en memoria
        for (const auto& mount : mountedDisks) {
            json item;
            item["id"] = mount.first;
            item["disk"] = mount.second;
            mountedList.push_back(item);
            
            // Construir lista de IDs para el mensaje
            if (!ids.empty()) ids += ", ";
            ids += mount.first;
        }
        
        // 3. Guardar los datos en result.data
        result.data["mounted"] = mountedList;
        
        // 4. Mensaje según PDF: mostrar los IDs
        if (mountedDisks.empty()) {
            result.message = "No hay particiones montadas";
        } else {

            result.message = ids;
        }
        
        result.success = true;
        
    } catch (const std::exception& e) {
        result.message = "Error al procesar MOUNTED: " + std::string(e.what());
    }
    
    return result;
}