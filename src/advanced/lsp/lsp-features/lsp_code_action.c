#include "lsp_code_action.h"
#include <stdlib.h>
#include <string.h>
#include "../../../environnement/constants.h"
#include "../../../environnement/global_variables.h"
#include "../../../terminal/windows/edw.h"
#include "../../../terminal/windows/pow.h"
#include "lsp_tools.h"

void receiveCodeActionData(cJSON* packet, FileContainer* file, ModuleContext* data) {
  cJSON* result = cJSON_GetObjectItem(packet, "result");
  if (result == NULL || cJSON_IsNull(result)) {
    return;
  }

  LSP_ComputedData* computed = file->lsp_datas.computed;

  // Clean up old code actions before storing new ones
  LSP_destroyCodeActionList(&computed->code_actions);

  // Parse new actions from JSON
  computed->code_actions.size = cJSON_GetArraySize(result);
  if (computed->code_actions.size == 0) {
    computed->code_actions.items = NULL;
    // if there is no data we close the popup
    if (computed->completions.completions.size == 0) {
      if (data->view_port.gui->edw_context.pow_owner == COMPLETION) {
        gui_closeEDWPopup(data->view_port.gui);
      }
    }
    return;
  }

  computed->code_actions.items = malloc(sizeof(LSP_CodeAction) * computed->code_actions.size);
  for (int i = 0; i < computed->code_actions.size; i++) {
    computed->code_actions.items[i] = LSP_getCodeActionFromJSON(cJSON_GetArrayItem(result, i));
  }

  // Update UI if COMPLETION popup is active, or open it if it's a direct code action request
  if (data->view_port.gui->edw_context.pow_owner == COMPLETION && data->view_port.gui->edw_context.show_pow) {
    gui_updateEDW(data->view_port.gui);
  }
  else {
    ViewPort view_port = viewPortOf(data->view_port.gui, &file->screen_x, &file->screen_y);
    // We use COMPLETION owner for the unified list
    gui_showGenericPopupWithTextAnchor(&view_port, data->cursor, computed->code_actions.size + 2, 45, COMPLETION,
                                       LF_tab_size(file->feature));
    gui_updateEDW(data->view_port.gui);
  }
}

void askCodeAction(FileContainer* file, Cursor* cursor) {
  if (!file->lsp_datas.is_enable) {
    return;
  }

  LSP_Server* lsp = getLSPServerForLanguage(&lsp_servers, file->lsp_datas.lang_id);
  if (lsp == NULL) {
    return;
  }

  LSP_ComputedData* computed = file->lsp_datas.computed;
  int lsp_row = cursor_row(*cursor) - 1;

  // Gathers up to 10 diagnostics overlapping the current line to help the server
  // find relevant quick-fixes.
  LSP_Diagnostic context_diags[10];
  int diags_count = 0;

  for (int i = 0; i < computed->diagnostics.size && diags_count < 10; i++) {
    LSP_Diagnostic* d = &computed->diagnostics.items[i];
    if (d->range.pos1.row <= lsp_row && d->range.pos2.row >= lsp_row) {
      context_diags[diags_count++] = *d;
    }
  }

  // Request code actions for the whole current line
  LSP_Position start_pos = LSP_pos(lsp_row, 0);
  LSP_Position end_pos = LSP_pos_from_cursor(lsp, goToEnd(*cursor));
  LSP_Range range = LSP_range(start_pos, end_pos);

  LSP_requestCodeAction(lsp, file->io_file.path_abs, range, context_diags, diags_count);
}

void executeCodeAction(FileContainer* fc, Cursor* cursor, LSP_CodeAction* action, ModuleContext* data) {
  if (!action) {
    return;
  }

  if (action->has_edit) {
    // Apply the WorkspaceEdit
    applyWorkspaceEdit(fc, cursor, &action->edit, data->payload_state_change);
  }

  if (action->has_command) {
    LSP_Server* lsp = getLSPServerForLanguage(&lsp_servers, fc->lsp_datas.lang_id);
    if (lsp) {
      LSP_requestExecuteCommand(lsp, fc->io_file.path_abs, &action->command);
    }
  }

  gui_updateEDW(data->view_port.gui);
}
