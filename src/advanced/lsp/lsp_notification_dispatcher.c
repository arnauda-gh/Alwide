#include "lsp_notification_dispatcher.h"

#include <assert.h>
#include <string.h>
#include "../../utils/logger.h"


void readPublishDiagnostic(cJSON* params, LSP_ComputedData* computed_data) {
  cJSON* diagnostics_item = cJSON_GetObjectItem(params, "diagnostics");

  // Clean previous diagnostics
  LSP_destroyDiagnosticList(&computed_data->diagnostics);

  computed_data->diagnostics.size = cJSON_GetArraySize(diagnostics_item);
  if (computed_data->diagnostics.size > 0) {
    computed_data->diagnostics.items = malloc(sizeof(LSP_Diagnostic) * computed_data->diagnostics.size);
    for (int i = 0; i < computed_data->diagnostics.size; i++) {
      computed_data->diagnostics.items[i] = LSP_getDiagnosticFromJSON(cJSON_GetArrayItem(diagnostics_item, i));
    }
  }
}


void notificationDispatcher(cJSON* packet, ModuleContext* data) {
  char* method = LSP_getPacketMethod(packet);
  cJSON* params = cJSON_GetObjectItem(packet, "params");

  // fetch computed data
  int index = getIndexFileContainerForUri(data, params);
  if (index == -1) {
    notifyUser(NULL, LOG_DEBUG, "LSP Notification: Couldn't find the file for the current packet.");
    return;
  }
  LSP_ComputedData* computed_data = (*data->files_state.files)[index].lsp_datas.computed;

  if (computed_data == NULL) {
    return; // Ignore stale notifications if LSP has been disabled/cleaned up for this buffer
  }

  if (strcmp(method, "textDocument/publishDiagnostics") == 0) {
    readPublishDiagnostic(params, computed_data);
  }
  else {
    notifyUser(NULL, LOG_DEBUG, "LSP Notification: Method NOT SUPPORTED '%s'.", method);
  }
}
