#include "lsp_completion.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "../../../environnement/global_variables.h"
#include "../../../io-management/viewport_history.h"
#include "../../../terminal/windows/edw.h"
#include "../../../terminal/windows/pow.h"
#include "lsp_code_action.h"
#include "lsp_signature_help.h"
#include "lsp_tools.h"


LSP_Range getReplaceRange(LSP_Server* lsp, Cursor* cursor, char insertText[METHOD_MAX_LENGTH]) {
  Cursor begin = *cursor;
  while (cursor_col(begin) != 0) {
    Cursor prev = moveLeft(begin);
    if (!isAWordLetter(getCharAtCursor(begin))) {
      break;
    }
    begin = prev;
  }

  return LSP_range_from_cursor(lsp, begin, *cursor);
}

void executeLSPCompletion(LSP_Server* lsp, Cursor* cursor, LSP_CompletionItem* item, History** history_p,
                          PayloadStateChange* payload_state_change, LF_Tabulation* tab) {
  if (!item->is_text_edit) {
    // we have to detect what to replace.
    item->text_edit.range = getReplaceRange(lsp, cursor, item->insertText);
    item->text_edit.new_text = strdup(item->insertText);
    item->is_text_edit = true;

    fprintf(stderr, "DEBUG Completion: Applying replacement range: [%d:%d] -> [%d:%d] with '%s'\n",
            item->text_edit.range.pos1.row, item->text_edit.range.pos1.column, item->text_edit.range.pos2.row,
            item->text_edit.range.pos2.column, item->text_edit.new_text);
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


static char current_prefix[METHOD_MAX_LENGTH] = "";

static int compareCompletionItems(const void* a, const void* b) {
  LSP_CompletionItem* itemA = (LSP_CompletionItem*)a;
  LSP_CompletionItem* itemB = (LSP_CompletionItem*)b;

  // Prefix matching scores
  int scoreA = 0;
  int scoreB = 0;

  if (strlen(current_prefix) > 0) {
    char* textA = strlen(itemA->filterText) > 0 ? itemA->filterText : itemA->label;
    char* textB = strlen(itemB->filterText) > 0 ? itemB->filterText : itemB->label;

    // Exact match (case sensitive)
    if (strcmp(textA, current_prefix) == 0) {
      scoreA = 1000;
    }
    if (strcmp(textB, current_prefix) == 0) {
      scoreB = 1000;
    }

    // Prefix match (case sensitive)
    if (scoreA == 0 && strncmp(textA, current_prefix, strlen(current_prefix)) == 0) {
      scoreA = 500;
    }
    if (scoreB == 0 && strncmp(textB, current_prefix, strlen(current_prefix)) == 0) {
      scoreB = 500;
    }

    // Prefix match (case insensitive)
    if (scoreA == 0 && strncasecmp(textA, current_prefix, strlen(current_prefix)) == 0) {
      scoreA = 250;
    }
    if (scoreB == 0 && strncasecmp(textB, current_prefix, strlen(current_prefix)) == 0) {
      scoreB = 250;
    }
  }

  if (scoreA != scoreB) {
    return scoreB - scoreA;
  }

  char* sortA = strlen(itemA->sortText) > 0 ? itemA->sortText : itemA->label;
  char* sortB = strlen(itemB->sortText) > 0 ? itemB->sortText : itemB->label;

  return strcmp(sortA, sortB);
}

void receiveCompletionData(cJSON* packet, FileContainer* file, ViewPort* view_port, Cursor* cursor) {
  LSP_destroyCompletionList(&file->lsp_datas.computed->completions);
  LSP_getCompletionListFromJSON(LSP_getPacketResult(packet), &file->lsp_datas.computed->completions);

  // Compute current prefix for sorting
  Cursor begin = *cursor;
  while (cursor_col(begin) != 0) {
    Cursor prev = moveLeft(begin);
    if (!isAWordLetter(getCharAtCursor(begin))) {
      break;
    }
    begin = prev;
  }


  char* prefix = dumpSelection(begin, *cursor);
  strncpy(current_prefix, prefix, METHOD_MAX_LENGTH - 1);
  current_prefix[METHOD_MAX_LENGTH - 1] = '\0';
  fprintf(stderr, "DEBUG Completion: Prefix identified: '%s'\n", current_prefix);
  free(prefix);

  // Sort completion items
  if (file->lsp_datas.computed->completions.completions.size > 1) {
    qsort(file->lsp_datas.computed->completions.completions.items,
          file->lsp_datas.computed->completions.completions.size, sizeof(LSP_CompletionItem), compareCompletionItems);
  }

  for (int i = 0; i < file->lsp_datas.computed->completions.completions.size && i < 5; i++) {
    LSP_CompletionItem* item = &file->lsp_datas.computed->completions.completions.items[i];
    fprintf(stderr, "DEBUG Completion Sort [%d]: %s (sortText: %s)\n", i, item->label, item->sortText);
  }

  // if there is no data we close the popup
  if (file->lsp_datas.computed->completions.completions.size == 0 && file->lsp_datas.computed->code_actions.size == 0) {
    if (view_port->gui->edw_context.pow_owner == COMPLETION) {
      gui_closePopup(view_port->gui);
    }
    return;
  }

  gui_resumeCompletionTextAnchor(view_port, cursor, LF_tab_size(file->feature));
}
