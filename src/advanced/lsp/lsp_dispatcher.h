#ifndef WISHWIM_LSP_DISPATCHER_H
#define WISHWIM_LSP_DISPATCHER_H

#include "../../../lib/cJSON/cJSON.h"

#include "../../data-management/file_management.h"
#include "../../io-management/viewport_history.h"
#include "../shared.h"

typedef struct ModuleContext {
  FilesState files_state;
  ViewPort view_port;
  Cursor* cursor;
  PayloadStateChange *payload_state_change;
} ModuleContext;


void dispatcher(cJSON* packet, LSP_Server* lsp, void* payload);

int getIndexFileContainerForUri(ModuleContext* payload, cJSON* params);

int getIndexFileContainerForName(ModuleContext* payload, char* file_name);

void printPacket(cJSON* packet, cJSON* params);

#endif // WISHWIM_LSP_DISPATCHER_H
