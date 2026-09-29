#include "command_handler.h"
CommandResult CommandHandler::processUnmount(const json& params) {
    (void)params;
    CommandResult result;
    result.success = false;
    result.message = "TODO: Implementar UNMOUNT";
    return result;
}
