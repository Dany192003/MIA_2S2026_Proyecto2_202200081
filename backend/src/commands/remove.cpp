#include "command_handler.h"
CommandResult CommandHandler::processRemove(const json& params) {
    (void)params;
    CommandResult result;
    result.success = false;
    result.message = "TODO: Implementar REMOVE";
    return result;
}
