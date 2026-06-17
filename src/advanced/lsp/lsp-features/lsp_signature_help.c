#include "lsp_signature_help.h"

#include <assert.h>
#include <string.h>
#include "../../../environnement/global_variables.h"
#include "../../../terminal/windows/edw.h"
#include "../../../terminal/windows/pow.h"
#include "../../../utils/tools.h"
#include "../lsp_handler.h"

void receiveSignatureHelpData(cJSON* packet, FileContainer* file, gui_Context* gui, Cursor* cursor) {
  LSP_destroySignatureHelp(&file->lsp_datas.computed->signature_help);
  LSP_getSignatureHelpFromJSON(LSP_getPacketResult(packet), &file->lsp_datas.computed->signature_help);

  if (file->lsp_datas.computed->signature_help.signatures_size == 0) {
    if (gui->edw_context.pow_owner == SIGNATURE_HELP) {
      gui_closeEDWPopup(gui);
    }
    return;
  }

  // Calculate dimensions using countStringFrame
  LSP_SignatureHelp* help = &file->lsp_datas.computed->signature_help;
  int sig_idx = help->activeSignature;
  if (sig_idx < 0 || sig_idx >= help->signatures_size) {
    sig_idx = 0;
  }
  LSP_SignatureInformation* sig = &help->signatures[sig_idx];

  int total_height = 0;
  int global_max_width = 0;
  int max_popup_width = 100;

  // Signature Label
  int label_rows = 0;
  int label_curr_col = 0;
  int label_max_w = max_popup_width;
  countStringFrame(sig->label, strlen(sig->label), &label_rows, &label_curr_col, &label_max_w, 2);
  total_height += (label_rows + 1);
  global_max_width = label_max_w;

  // Documentation
  if (sig->documentation[0] != '\0') {
    total_height++; // Separator line
    int doc_rows = 0;
    int doc_curr_col = 0;
    int doc_max_w = max_popup_width;
    countStringFrame(sig->documentation, strlen(sig->documentation), &doc_rows, &doc_curr_col, &doc_max_w, 2);
    total_height += (doc_rows + 1);
    if (doc_max_w > global_max_width) {
      global_max_width = doc_max_w;
    }
  }

  int width = global_max_width;
  int height = total_height + 2; // +2 for spacing if doc exists, or just breathing room

  // Constrain dimensions
  if (width < 30) {
    width = 30;
  }
  if (height > 15) {
    height = 15;
  }

  // Align '(' with editor
  char* paren_in_sig = strchr(sig->label, '(');
  int paren_offset_in_sig = paren_in_sig ? (paren_in_sig - sig->label) : 0;

  // Find the last '(' before the cursor on the current line to anchor the popup
  int paren_col = -1;
  LineIdentifier it = cursor->line_id;
  while (hasElementBeforeLine(it)) {
    Char_U8 ch = getCharForLineIdentifier(it);
    if (ch.t[0] == '(' && sizeChar_U8(ch) == 1) {
      paren_col = it.absolute_column;
      break;
    }
    it.relative_column--;
    it.absolute_column--;
  }

  ViewPort view_port = viewPortOf(gui, &file->screen_x, &file->screen_y);
  int popup_x;
  if (paren_col != -1) {
    Cursor paren_cursor = *cursor;
    paren_cursor.line_id.absolute_column = paren_col;
    int paren_screen_x = getScreenXForCursor(paren_cursor, *view_port.screen_x, LF_tab_size(file->feature));
    popup_x = paren_screen_x - paren_offset_in_sig - 1;
  }
  else {
    // Fallback if no '(' found (shouldn't happen for signature help)
    int cursor_screen_x = getScreenXForCursor(*cursor, *view_port.screen_x, LF_tab_size(file->feature));
    popup_x = cursor_screen_x - 1 - paren_offset_in_sig;
  }

  gui_showGenericPopup(gui, cursor_row(*cursor) - *view_port.screen_y + 1, popup_x, height, width, SIGNATURE_HELP);
  gui_updateEDW(gui);
}

void askSignatureHelp(FileContainer* file, Cursor* cursor) {
  if (!file->lsp_datas.is_enable) {
    return;
  }

  LSP_Server* lsp = getLSPServerForLanguage(&lsp_servers, file->lsp_datas.lang_id);
  if (lsp == NULL) {
    return;
  }

  LSP_requestSignatureHelp(lsp, file->io_file.path_abs, LSP_pos_from_cursor(lsp, *cursor));
}


bool adaptSignatureHelpOnDelete(Cursor cursor, Cursor select_cursor, LSP_Data* lsp_data, EditorContext* ctx) {
  if (!lsp_data->is_enable) {
    return false;
  }

  if (ctx->gui_context.edw_context.pow_owner != SIGNATURE_HELP) {
    return false;
  }

  if (!cursor_le(select_cursor, cursor)) {
    Cursor tmp = select_cursor;
    select_cursor = cursor;
    cursor = tmp;
  }

  // DELETE_ONE Action
  if (cursor_eq(select_cursor, moveLeft(cursor))) {

    // assert that we have a char under the cursor.
    if (cursor_col(cursor) != 0) {
      Char_U8 u8 = getCharAtCursor(cursor);

      if (areChar_U8Equals(u8, readChar_U8FromCharArray(",")) &&
          lsp_data->computed->signature_help.activeParameter > 0) {
        // We detect that it
        lsp_data->computed->signature_help.activeParameter--;
        return true;
      }

      if (areChar_U8Equals(u8, readChar_U8FromCharArray("("))) {
        gui_closeEDWPopup(&ctx->gui_context);
      }
    }
  }

  return false;
}

bool askSignatureHelpOnChar(EditorContext* ctx, int c, FileContainer* fc, Cursor* cursor) {
  if (c == '(' || c == ',') {
    askSignatureHelp(fc, cursor);
    return true;
  }

  if (c == ')' && ctx->gui_context.edw_context.pow_owner == SIGNATURE_HELP) {
    gui_closeEDWPopup(&ctx->gui_context);
  }

  return false;
}
