#include "comments.h"
#include <ctype.h>
#include <string.h>
#include "../../data-management/state_control.h"
#include "../../environnement/global_variables.h"

static bool isLineCommented(LineNode* line, const char* prefix) {
  if (!prefix || strlen(prefix) == 0) {
    return false;
  }

  // Skip leading whitespace
  int i = 0;
  while (i < line->element_number && (line->ch[i].t[0] == ' ' || line->ch[i].t[0] == '\t')) {
    i++;
  }

  if (i >= line->element_number) {
    return false; // Empty or whitespace only
  }

  // Check for prefix
  int prefix_len = strlen(prefix);
  if (line->element_number - i < prefix_len) {
    return false;
  }

  for (int j = 0; j < prefix_len; j++) {
    if (line->ch[i + j].t[0] != prefix[j]) {
      return false;
    }
  }

  return true;
}

void ilj_toggleComments(FileContainer* fc, History** history_frame, PayloadStateChange* payload_state_change) {
  if (!fc->feature || fc->feature->comments.line[0] == '\0') {
    return;
  }

  // Save current selection/cursor state
  Cursor original_cursor = fc->cursor;
  Cursor original_select = fc->select_cursor;

  // Normalize range
  Cursor start = original_cursor;
  Cursor end = original_select;
  if (cursor_is_disabled(end)) {
    end = start;
  }
  else if (cursor_gt(start, end)) {
    Cursor tmp = start;
    start = end;
    end = tmp;
  }

  int start_row = start.file_id.absolute_row;
  int end_row = end.file_id.absolute_row;
  const char* prefix = fc->feature->comments.line;
  int prefix_len = strlen(prefix);

  // 1. Determine if we should comment or uncomment
  bool all_commented = true;
  for (int r = start_row; r <= end_row; r++) {
    Cursor it = tryToReachAbsPosition(original_cursor, r, 0);
    LineNode* line = getLineForFileIdentifier(it.file_id);

    bool empty = true;
    for (int i = 0; i < line->element_number; i++) {
      if (!isspace(line->ch[i].t[0])) {
        empty = false;
        break;
      }
    }

    if (!empty && !isLineCommented(line, prefix)) {
      all_commented = false;
      break;
    }
  }

  int cursor_row = original_cursor.file_id.absolute_row;
  int cursor_col = original_cursor.line_id.absolute_column;
  int select_row = cursor_is_disabled(original_select) ? -1 : original_select.file_id.absolute_row;
  int select_col = cursor_is_disabled(original_select) ? -1 : original_select.line_id.absolute_column;

  // 2. Apply change using high-level functions
  for (int r = start_row; r <= end_row; r++) {
    Cursor it = tryToReachAbsPosition(original_cursor, r, 0);
    LineNode* line = getLineForFileIdentifier(it.file_id);

    // Find indentation
    int indent = 0;
    while (indent < line->element_number && (line->ch[indent].t[0] == ' ' || line->ch[indent].t[0] == '\t')) {
      indent++;
    }

    if (all_commented) {
      if (isLineCommented(line, prefix)) {
        Cursor delete_start = tryToReachAbsPosition(it, r, indent);
        Cursor delete_end = tryToReachAbsPosition(it, r, indent + prefix_len);
        deleteSelectionWithState(history_frame, &delete_start, &delete_end, payload_state_change);

        // Adjust column position if cursor or selection was on this line
        if (r == cursor_row) {
          if (cursor_col >= indent + prefix_len) {
            cursor_col -= prefix_len;
          }
          else if (cursor_col > indent) {
            cursor_col = indent;
          }
        }
        if (r == select_row) {
          if (select_col >= indent + prefix_len) {
            select_col -= prefix_len;
          }
          else if (select_col > indent) {
            select_col = indent;
          }
        }
      }
    }
    else {
      // Don't comment purely empty lines unless it's the only line
      bool empty = true;
      for (int i = 0; i < line->element_number; i++) {
        if (!isspace(line->ch[i].t[0])) {
          empty = false;
          break;
        }
      }

      if (!empty || start_row == end_row) {
        Cursor insert_pos = tryToReachAbsPosition(it, r, indent);
        insert_pos = insertCharArrayAtCursorWithState(history_frame, insert_pos, (char*)prefix, payload_state_change,
                                                      LF_tab(fc->feature));

        // Adjust column position if cursor or selection was on this line
        if (r == cursor_row) {
          if (cursor_col >= indent) {
            cursor_col += prefix_len;
          }
        }
        if (r == select_row) {
          if (select_col >= indent) {
            select_col += prefix_len;
          }
        }
      }
    }
  }

  // Restore cursor/selection
  fc->cursor = tryToReachAbsPosition(original_cursor, cursor_row, cursor_col);
  fc->desired_column = cursor_col;

  if (!cursor_is_disabled(original_select)) {
    fc->select_cursor = tryToReachAbsPosition(original_cursor, select_row, select_col);
  }

  // Jump to the line under if called on only one line
  if (start_row == end_row) {
    if (hasElementAfterFile(fc->cursor.file_id)) {
      fc->cursor = moveDown(fc->cursor, fc->desired_column);
      setSelectCursorOff(&fc->cursor, &fc->select_cursor, SELECT_OFF_RIGHT);
    }
  }
}
