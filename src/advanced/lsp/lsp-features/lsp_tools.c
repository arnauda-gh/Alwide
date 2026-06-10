#include "lsp_tools.h"

#include <string.h>
#include "../../../data-management/encoding/utf16.h"
#include "../../../data-management/file_management.h"
#include "../../../environnement/global_variables.h"

void applyTextEdit(LSP_Server* lsp, Cursor* cursor, LSP_TextEdit* text_edit, History** history_p,
                   PayloadStateChange *payload_state_change, LF_Tabulation* tab) {
  // As a text edit can represent a "replacement" we have to handle this deleting old text and inserting new text after.
  // Delete part
  *cursor = LSP_tryToReachCursorForLSPPosition(lsp, *cursor, text_edit->range.pos1);
  Cursor end = LSP_tryToReachCursorForLSPPosition(lsp, *cursor, text_edit->range.pos2);
  deleteSelectionWithState(history_p, cursor, &end, payload_state_change);
  // insert part
  *cursor = insertCharArrayAtCursorWithState(history_p, *cursor, text_edit->new_text, payload_state_change, tab);
}

void applyTextEditsArray(LSP_Server* lsp, Cursor* cursor, LSP_TextEdit* edits, int edits_size, History** history_p,
                         PayloadStateChange *payload_state_change, LF_Tabulation* tab) {
  if (edits_size <= 0) {
    return;
  }

  // Sort edits from bottom to top to avoid offset issues
  qsort(edits, edits_size, sizeof(LSP_TextEdit), compareTextEdit);

  // Tracker for the cursor position (0-based LSP)
  LSP_Position tracker = LSP_pos_from_cursor(lsp, *cursor);

  for (int i = 0; i < edits_size; i++) {
    LSP_Position edit_start = edits[i].range.pos1;
    LSP_Position edit_end = edits[i].range.pos2;
    LSP_Position new_text_end = calculateEndPos(lsp, edit_start, edits[i].new_text);

    // Apply the edit
    applyTextEdit(lsp, cursor, &edits[i], history_p, payload_state_change, tab);

    // Track the cursor shift
    if (compareLSPPos(tracker, edit_end) >= 0) {
      // Tracker is AFTER or AT the end of the original edit range
      if (tracker.row == edit_end.row) {
        // Tracker is on the same line as the end of the edit
        tracker.column = new_text_end.column + (tracker.column - edit_end.column);
      }
      tracker.row += (new_text_end.row - edit_end.row);
    }
    else if (compareLSPPos(tracker, edit_start) > 0) {
      // Tracker is INSIDE the edit range
      // Move tracker to the end of the new text
      tracker = new_text_end;
    }
  }

  // Restore cursor at tracked position
  *cursor = LSP_tryToReachCursorForLSPPosition(lsp, *cursor, tracker);
}

void applyWorkspaceEdit(FileContainer* fc, Cursor* cursor, LSP_WorkspaceEdit* ws_edit,
                        PayloadStateChange *payload_state_change) {
  if (!ws_edit || ws_edit->document_changes_count <= 0) {
    return;
  }

  LSP_Server* lsp = getLSPServerForLanguage(&lsp_servers, fc->lsp_datas.lang_id);

  for (int i = 0; i < ws_edit->document_changes_count; i++) {
    LSP_TextDocumentEdit* doc_edit = &ws_edit->document_changes[i];

    // For now, we only apply edits if they match the current file.
    // !! REALLY important when using WorkspaceEdit !! TODO In the future, we could find the FileContainer by
    // text_document.file_name URI.
    if (strcmp(doc_edit->text_document.file_name, fc->io_file.path_abs) == 0) {
      applyTextEditsArray(lsp, cursor, doc_edit->edits, doc_edit->edits_count, &fc->history_frame, payload_state_change,
                          LF_tab(fc->feature));
    }
  }
}

int compareTextEdit(const void* e1_p, const void* e2_p) {
  LSP_TextEdit* e1 = (LSP_TextEdit*)e1_p;
  LSP_TextEdit* e2 = (LSP_TextEdit*)e2_p;
  if (e1->range.pos1.row < e2->range.pos1.row) {
    return 1;
  }
  if (e1->range.pos1.row > e2->range.pos1.row) {
    return -1;
  }

  return e1->range.pos1.column <= e2->range.pos1.column ? 1 : -1;
}

int compareLSPPos(LSP_Position p1, LSP_Position p2) {
  if (p1.row < p2.row) {
    return -1;
  }
  if (p1.row > p2.row) {
    return 1;
  }
  if (p1.column < p2.column) {
    return -1;
  }
  if (p1.column > p2.column) {
    return 1;
  }
  return 0;
}

LSP_Position calculateEndPos(LSP_Server* server, LSP_Position start, const char* text) {
  LSP_Position end = start;
  if (!text) {
    return end;
  }
  int index = 0;
  while (text[index]) {
    if (text[index] == '\n') {
      end.row++;
      end.column = 0;
      index++;
    }
    else {
      Char_U8 u8 = readChar_U8FromCharArrayWithFirst((char*)text + index, text[index]);
      int sz = utf8_size(u8);
      index += sz;
      if (server != NULL && server->position_encoding == LSP_POSITION_ENCODING_UTF32) {
        end.column += 1;
      }
      else if (server != NULL && server->position_encoding == LSP_POSITION_ENCODING_UTF8) {
        end.column += sz;
      }
      else {
        end.column += utf16_length(u8);
      }
    }
  }
  return end;
}
