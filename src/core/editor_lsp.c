#include "editor_lsp.h"

#include <assert.h>
#include <ncurses.h>
#include <unistd.h>

#include "../advanced/lsp/lsp-features/lsp_completion.h"
#include "../advanced/lsp/lsp-features/lsp_signature_help.h"
#include "../advanced/lsp/lsp_client.h"
#include "../environnement/global_variables.h"
#include "../terminal/key_management.h"
#include "../utils/tools.h"

ModuleContext buildModuleContext(EditorContext* ctx) {
  FileContainer* fc = &ctx->files[ctx->current_file_index];
  ModuleContext payload;
  payload.files_state = filesStateOf(&ctx->files, &ctx->file_count, &ctx->current_file_index, &ctx->refresh_local_vars);
  payload.view_port = viewPortOf(&ctx->gui_context, &fc->screen_x, &fc->screen_y);
  payload.cursor = &fc->cursor;
  payload.payload_state_change = &ctx->payload_state_change;
  return payload;
}

void handleLspServers(EditorContext* ctx, int* key) {
  LSPServerLinkedList_Cell* cell = lsp_servers.head;
  while (cell != NULL) {
    ModuleContext payload = buildModuleContext(ctx);
    while (LSP_dispatchOnReceive(&cell->lsp_server, dispatcher, &payload)) {
      // Refresh payload in case of realloc during dispatch
      if (*key == ERR) {
        *key = ONLY_REPAINT_INPUT;
      }
      payload = buildModuleContext(ctx);
    }
    cell = cell->next;
  }
}

void waitForLspResponse(EditorContext* ctx, int timeout_ms) {
  time_val start = timeInMilliseconds();
  while (diff2Time(timeInMilliseconds(), start) < timeout_ms) {
    int key = ERR;
    handleLspServers(ctx, &key);
    usleep(10000); // 10ms
  }
}

void askOnCharTypeLspInfos(EditorContext* ctx, int key, FileContainer* fc, Cursor* cursor) {
  /* Only ask for LSP info on pure printable characters (Bits 0-23) */
  int codepoint = K_CODE(key);
  assert(codepoint == key);
  bool hasAsked = askSignatureHelpOnChar(ctx, codepoint, fc, cursor); // priority 1
  if (hasAsked) {
    return;
  }
  askCompletion(&ctx->gui_context, fc, false, false); // priority 2
}
