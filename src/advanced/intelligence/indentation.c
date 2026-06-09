#include "indentation.h"
#include <string.h>
#include "../../data-management/state_control.h"
#include "../../environnement/global_variables.h"

void ilf_indentSelectedLines(FileContainer* fc, History** history_frame, PayloadStateChange* payload_state_change) {
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

  int cursor_row = original_cursor.file_id.absolute_row;
  int cursor_col = original_cursor.line_id.absolute_column;
  int select_row = cursor_is_disabled(original_select) ? -1 : original_select.file_id.absolute_row;
  int select_col = cursor_is_disabled(original_select) ? -1 : original_select.line_id.absolute_column;

  bool use_space = LF_tab_use_space(fc->feature);
  int tab_size = LF_tab_size(fc->feature);
  LF_Tabulation* tab = LF_tab(fc->feature);

  char indent_str[16] = {0};
  int shift_amount = 0;

  if (use_space) {
    int limit = (tab_size < 15) ? tab_size : 15;
    for (int i = 0; i < limit; i++) {
      indent_str[i] = ' ';
    }
    shift_amount = limit;
  }
  else {
    indent_str[0] = '\t';
    shift_amount = 1;
  }

  for (int r = start_row; r <= end_row; r++) {
    Cursor it = tryToReachAbsPosition(original_cursor, r, 0);
    insertCharArrayAtCursorWithState(history_frame, it, indent_str, payload_state_change, tab);

    // Adjust column positions
    if (r == cursor_row) {
      cursor_col += shift_amount;
    }
    if (r == select_row) {
      select_col += shift_amount;
    }
  }

  // Restore cursor/selection
  fc->cursor = tryToReachAbsPosition(original_cursor, cursor_row, cursor_col);
  if (!cursor_is_disabled(original_select)) {
    fc->select_cursor = tryToReachAbsPosition(original_cursor, select_row, select_col);
  }
}

void ilf_deindentSelectedLines(FileContainer* fc, History** history_frame, PayloadStateChange* payload_state_change) {
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

  int cursor_row = original_cursor.file_id.absolute_row;
  int cursor_col = original_cursor.line_id.absolute_column;
  int select_row = cursor_is_disabled(original_select) ? -1 : original_select.file_id.absolute_row;
  int select_col = cursor_is_disabled(original_select) ? -1 : original_select.line_id.absolute_column;

  int tab_size = LF_tab_size(fc->feature);

  for (int r = start_row; r <= end_row; r++) {
    Cursor it = tryToReachAbsPosition(original_cursor, r, 0);
    LineNode* line = getLineForFileIdentifier(it.file_id);

    int to_delete = 0;
    if (line->element_number > 0) {
      if (line->ch[0].t[0] == '\t') {
        to_delete = 1;
      }
      else if (line->ch[0].t[0] == ' ') {
        int space_count = 0;
        while (space_count < line->element_number && space_count < tab_size && line->ch[space_count].t[0] == ' ') {
          space_count++;
        }
        to_delete = space_count;
      }
    }

    if (to_delete > 0) {
      Cursor delete_start = tryToReachAbsPosition(it, r, 0);
      Cursor delete_end = tryToReachAbsPosition(it, r, to_delete);
      deleteSelectionWithState(history_frame, &delete_start, &delete_end, payload_state_change);

      // Adjust column positions
      if (r == cursor_row) {
        cursor_col -= to_delete;
        if (cursor_col < 0) {
          cursor_col = 0;
        }
      }
      if (r == select_row) {
        select_col -= to_delete;
        if (select_col < 0) {
          select_col = 0;
        }
      }
    }
  }

  // Restore cursor/selection
  fc->cursor = tryToReachAbsPosition(original_cursor, cursor_row, cursor_col);
  if (!cursor_is_disabled(original_select)) {
    fc->select_cursor = tryToReachAbsPosition(original_cursor, select_row, select_col);
  }
}
