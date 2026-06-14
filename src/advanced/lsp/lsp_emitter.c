#include "lsp_emitter.h"

#include <limits.h>
#include "../../data-management/encoding/utf16.h"
#include "../../environnement/global_variables.h"
#include "../../terminal/key_management.h"
#include "../../utils/tools.h"

int global_version = 2;

static int getLSPColumnFromCursor(LSP_Server* server, Cursor cursor, int row, int character_column) {
  Cursor target = tryToReachAbsPosition(cursor, row + 1, character_column);
  return LSP_pos_from_cursor(server, target).column;
}

static int getLSPCharLen(LSP_Server* server, Char_U8 u8) {
  if (server->position_encoding == LSP_POSITION_ENCODING_UTF32) {
    return 1;
  }
  if (server->position_encoding == LSP_POSITION_ENCODING_UTF8) {
    return utf8_size(u8);
  }
  return utf16_length(u8);
}

void onStateChangeLSP(Action action, LSP_Data* data, Cursor* cursor) {
  if (!data->is_enable) {
    return;
  }
  LSP_Server* lsp_server = getLSPServerForLanguage(&lsp_servers, data->lang_id);
  if (lsp_server == NULL) {
    return;
  }

  ChangeDescriptor action_change = actionToChangeDescriptor(action);

  int start_lsp_col =
    getLSPColumnFromCursor(lsp_server, *cursor, action_change.start_point.row, action_change.start_point.column);
  int old_end_lsp_col = start_lsp_col;

  if (action.action == INSERT) {
    old_end_lsp_col = start_lsp_col;
  }
  else if (action.action == DELETE || action.action == DELETE_ONE) {
    if (action.action == DELETE) {
      int rows_deleted = 0;
      int last_line_lsp_len = 0;
      int index = 0;
      int length = action.byte_end - action.byte_start;
      while (index < length && action.ch[index] != '\0') {
        if (action.ch[index] == '\n') {
          rows_deleted++;
          last_line_lsp_len = 0;
          index++;
        }
        else {
          Char_U8 u8 = readChar_U8FromCharArrayWithFirst((char*)action.ch + index, action.ch[index]);
          index += utf8_size(u8);
          last_line_lsp_len += getLSPCharLen(lsp_server, u8);
        }
      }

      if (rows_deleted > 0) {
        old_end_lsp_col = last_line_lsp_len;
      }
      else {
        old_end_lsp_col = start_lsp_col + last_line_lsp_len;
      }
    }
    else {
      Char_U8 u8 = unicode_to_utf8(K_CODE(action.unique_ch));
      if (action.unique_ch == '\n') {
        old_end_lsp_col = 0;
      }
      else {
        old_end_lsp_col = start_lsp_col + getLSPCharLen(lsp_server, u8);
      }
    }
  }

  // Create array
  cJSON* array_of_changes = cJSON_CreateArray();

  if (lsp_server->sync_kind == 1) { // Full synchronization
    cJSON* change = cJSON_CreateObject();
    char* full_text =
      dumpSelection(tryToReachAbsPosition(*cursor, 1, 0), tryToReachAbsPosition(*cursor, INT_MAX, INT_MAX));
    cJSON_AddStringToObject(change, "text", full_text);
    free(full_text);
    cJSON_AddItemToArray(array_of_changes, change);
  }
  else { // Default to Incremental (2) or assume it's supported if not 0
    // Create the object that the array will contain, describe de action_change
    cJSON* change = cJSON_CreateObject();
    // Add the 'range' field to the change
    LSP_Range lsp_range = LSP_range(LSP_pos(action_change.start_point.row, start_lsp_col),
                                    LSP_pos(action_change.old_end_point.row, old_end_lsp_col));
    cJSON* range = LSP_getJSONRange(lsp_range);
    cJSON_AddItemToObject(change, "range", range);
    // Add the 'text' field to the change
    Cursor begin =
      tryToReachAbsPosition(*cursor, LSP_0_row_to_1_row(action_change.start_point.row), action_change.start_point.column);
    Cursor end = tryToReachAbsPosition(*cursor, LSP_0_row_to_1_row(action_change.new_end_point.row),
                                       action_change.new_end_point.column);
    char* new_text = dumpSelection(begin, end);
    cJSON_AddStringToObject(change, "text", new_text);
    free(new_text);

    // Add the change to the array
    cJSON_AddItemToArray(array_of_changes, change);
  }

  // Send the changes to the lsp server
  LSP_notifyLspFileDidChange(lsp_server, data->path_abs, array_of_changes, global_version++);
}
