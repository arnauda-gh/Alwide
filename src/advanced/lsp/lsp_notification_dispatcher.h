#ifndef WISHWIM_LSP_DIAGNOSTIC_HANDLER_H
#define WISHWIM_LSP_DIAGNOSTIC_HANDLER_H

#include <cjson/cJSON.h>
#include "lsp_dispatcher.h"


void notificationDispatcher(cJSON* packet, ModuleContext* data);

#endif // WISHWIM_LSP_DIAGNOSTIC_HANDLER_H
