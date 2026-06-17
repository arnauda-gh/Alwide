#include "lsp_response_dispatcher.h"

#include <assert.h>
#include <string.h>
#include "../../utils/logger.h"

#include "../../terminal/term_handler.h"
#include "../../terminal/windows/edw.h"
#include "../../terminal/windows/pow.h"
#include "lsp-features/lsp_code_action.h"
#include "lsp-features/lsp_completion.h"
#include "lsp-features/lsp_formatting.h"
#include "lsp-features/lsp_goto.h"
#include "lsp-features/lsp_hover.h"
#include "lsp-features/lsp_signature_help.h"


void responseDispatcher(cJSON* packet, LSP_Server* lsp, ModuleContext* data) {
  LSP_PacketID id = LSP_getPacketID(packet);

  LSP_ResponseContext context;
  bool pop_result = LSP_popResponseContext(lsp, id, &context);
  assert(pop_result);

  int index = getIndexFileContainerForName(data, context.file_name);
  if (index == -1) {
    notifyUser(NULL, LOG_DEBUG, "LSP Response: Couldn't find the file for the current response.");
    return;
  }

  FileContainer* file = (*data->files_state.files) + index;

  if (cJSON_GetObjectItem(packet, "error")) {
    if (strcmp(context.method, "textDocument/completion") == 0) {
      if (data->view_port.gui->edw_context.pow_owner == COMPLETION) {
        gui_closeEDWPopup(data->view_port.gui);
      }
    }
    fprintf(stderr, "LSP : ERROR RECEIVED from %s !\n    => Method issue : %s.\n    => Error message : %s\n",
            lsp->language, context.method,
            cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetObjectItem(packet, "error"), "message")));
    return;
  }

  if (strcmp(context.method, "textDocument/completion") == 0) {
    receiveCompletionData(packet, file, &data->view_port, data->cursor);
  }
  else if (strcmp(context.method, "textDocument/formatting") == 0 ||
           strcmp(context.method, "textDocument/onTypeFormatting") == 0) {
    receiveFormattingData(packet, file, data);
  }
  else if (strcmp(context.method, "textDocument/hover") == 0) {
    receiveHoverData(packet, file, &data->view_port, data->cursor, context.payload);
  }
  else if (strcmp(context.method, "textDocument/signatureHelp") == 0) {
    receiveSignatureHelpData(packet, file, data->view_port.gui, data->cursor);
  }
  else if (strcmp(context.method, "textDocument/codeAction") == 0) {
    receiveCodeActionData(packet, file, data);
  }
  else if (strcmp(context.method, "workspace/executeCommand") == 0) {
    // Command execution response received.
    // Usually, the server sends a workspace/applyEdit request separately.
    // We just acknowledge the response here.
    fprintf(stderr, "LSP : Command executed successfully.\n");
  }
  else if (strcmp(context.method, "textDocument/definition") == 0 ||
           strcmp(context.method, "textDocument/declaration") == 0 ||
           strcmp(context.method, "textDocument/typeDefinition") == 0 ||
           strcmp(context.method, "textDocument/implementation") == 0 ||
           strcmp(context.method, "textDocument/references") == 0) {
    receiveGotoData(packet, lsp, file, data, &file->lsp_datas.computed->gotos, context.method, context.payload);
  }
  else {
    notifyUser(NULL, LOG_DEBUG, "LSP Response: Method NOT SUPPORTED '%s'.", context.method);
  }
}
