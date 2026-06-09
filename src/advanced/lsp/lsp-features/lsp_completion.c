#include "lsp_completion.h"

#include <assert.h>
#include <string.h>

#include "../../../environnement/global_variables.h"
#include "../../../io-management/viewport_history.h"
#include "../../../terminal/windows/edw.h"
#include "../../../terminal/windows/pow.h"
#include "lsp_code_action.h"
#include "lsp_signature_help.h"
#include "lsp_tools.h"


LSP_Range getReplaceRange(LSP_Server* lsp, Cursor* cursor, char insertText[METHOD_MAX_LENGTH]) {
  Cursor select = cursor_disable(*cursor);
  if (cursor->line_id.absolute_column != 0) {
    Cursor tmp = *cursor;
    selectWord(&tmp, &select);
  }

  Cursor begin = select;
  int index = 0;

  bool begin_matching = true;
  while (!cursor_is_disabled(select) && cursor_lt(select, *cursor)) {
    select = moveRight(select);
    Char_U8 ch = getCharAtCursor(select);
    for (int i = 0; i < sizeChar_U8(ch); i++) {
      assert(index <= METHOD_MAX_LENGTH);
      if (insertText[index] != ch.t[i] && towupper(insertText[index]) != towupper(ch.t[i])) {
        begin_matching = false;
        break;
      }
      index++;
    }
  }

  if (cursor_is_disabled(select) || begin_matching == false) {
    begin = *cursor;
  }

  return LSP_range_from_cursor(lsp, begin, *cursor);
}

void executeLSPCompletion(LSP_Server* lsp, Cursor* cursor, LSP_CompletionItem* item, History** history_p,
                          PayloadStateChange *payload_state_change, LF_Tabulation* tab) {
  if (!item->is_text_edit) {
    // copy the text to the edit.
    item->text_edit.new_text = malloc(sizeof(char) * METHOD_MAX_LENGTH);
    memcpy(item->text_edit.new_text, item->insertText, sizeof(char) * METHOD_MAX_LENGTH);

    // we have to detect what to replace.
    item->text_edit.range = getReplaceRange(lsp, cursor, item->insertText);
    item->is_text_edit = true;
  }

  // Combine main text_edit and additionalTextEdits into a single array for applyTextEditsArray
  int total_edits_count = 1 + item->additionalTextEditsSize;
  LSP_TextEdit* all_edits = malloc(sizeof(LSP_TextEdit) * total_edits_count);

  all_edits[0] = item->text_edit;
  for (int i = 0; i < item->additionalTextEditsSize; i++) {
    all_edits[i + 1] = item->additionalTextEdits[i];
  }

  // Use the robust generic application tool
  applyTextEditsArray(lsp, cursor, all_edits, total_edits_count, history_p, payload_state_change, tab);

  free(all_edits);
}


static bool checkLineHasDiagnostics(LSP_ComputedData* computed, int row) {
  for (int i = 0; i < computed->diagnostics.size; i++) {
    if (computed->diagnostics.items[i].range.pos1.row <= row && computed->diagnostics.items[i].range.pos2.row >= row) {
      return true;
    }
  }
  return false;
}


void askCompletion(gui_Context* gui_context, FileContainer* fc, bool reset, bool force) {
  if (fc->lsp_datas.is_enable) {
    LSP_Server* lsp = getLSPServerForLanguage(&lsp_servers, fc->lsp_datas.lang_id);
    if (lsp == NULL) {
      return;
    }

    // check if the askCompletion have to be replaced by a askSignature.
    if (hasElementBeforeLine(fc->cursor.line_id)) {
      Char_U8 u8 = getCharAtCursor(skipLeftInvisibleChar(fc->cursor));
      if (areChar_U8Equals(u8, readChar_U8FromCharArray("(")) || areChar_U8Equals(u8, readChar_U8FromCharArray(","))) {
        askSignatureHelp(fc, &fc->cursor);
        return;
      }
    }

    // Don't override signature help for non forced trigger.
    if (!force && gui_context->edw_context.pow_owner == SIGNATURE_HELP) {
      return;
    }

    // if it's not a force don't auto trigger if it's not before a word.
    if (!force && !isAfterAWord(&fc->cursor)) {
      LSP_destroyCompletionList(&fc->lsp_datas.computed->completions);
      gui_closePopup(gui_context);
      return;
    }

    if (reset) {
      LSP_destroyCompletionList(&fc->lsp_datas.computed->completions);
      LSP_destroyCodeActionList(&fc->lsp_datas.computed->code_actions);
    }

    // If a line have a diagnostic so we ask for code action.
    if (checkLineHasDiagnostics(fc->lsp_datas.computed, cursor_row(fc->cursor) - 1)) {
      askCodeAction(fc, &fc->cursor);
    }

    LSP_requestCompletion(lsp, fc->lsp_datas.path_abs, LSP_pos_from_cursor(lsp, fc->cursor));
    if (gui_context->edw_context.pow_owner != COMPLETION) {
      gui_setLastTextAnchor(gui_context, cursor_to_desc(fc->cursor));
    }
    else {
      ViewPort view_port = viewPortOf(gui_context, &fc->screen_x, &fc->screen_y);
      gui_showGenericPopupWithTextAnchor(&view_port, &fc->cursor, 7, 50, COMPLETION, LF_tab_size(fc->feature));
    }
  }
}

void receiveCompletionData(cJSON* packet, FileContainer* file, ViewPort* view_port, Cursor* cursor) {
  LSP_destroyCompletionList(&file->lsp_datas.computed->completions);
  LSP_getCompletionListFromJSON(LSP_getPacketResult(packet), &file->lsp_datas.computed->completions);

  // if there is no data we close the popup
  if (file->lsp_datas.computed->completions.completions.size == 0 && file->lsp_datas.computed->code_actions.size == 0) {
    if (view_port->gui->edw_context.pow_owner == COMPLETION) {
      gui_closePopup(view_port->gui);
    }
    return;
  }

  gui_resumeCompletionTextAnchor(view_port, cursor, LF_tab_size(file->feature));
}
