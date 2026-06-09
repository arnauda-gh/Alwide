#include "text_box.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "../../../environnement/constants.h"
#include "../../../terminal/term_handler.h"
#include "../../../utils/tools.h"

void renderTextBuffer(WINDOW* w, TextBuffer* tb, int offset_x, int offset_y, int line_length, int max_line_number) {
  Cursor start = tryToReachAbsPosition(tb->cursor, 1, 0);
  Cursor end = tryToReachAbsPosition(tb->cursor, INT_MAX, INT_MAX);
  char* full_text = dumpSelection(start, end);

  int tab_size = tb->feature ? tb->feature->tabulation.size : 4;
  if (tab_size <= 0) tab_size = 4;

  // 1. Draw the text buffer using printToWindow
  printToWindow(w, full_text, -1, offset_x, offset_y, line_length, max_line_number, tab_size);

  // 2. Draw selection or simulated cursor
  char* prefix = dumpSelection(start, tb->cursor);
  int cur_r = 0;
  int cur_c = 0;
  int wrap_w = line_length;
  countStringFrame(prefix, strlen(prefix), &cur_r, &cur_c, &wrap_w, tab_size);

  if (cursor_is_disabled(tb->select_cursor)) {
    // Single cursor
    int target_x = offset_x + cur_c;
    int target_y = offset_y + cur_r;
    // Ensure we don't draw out of bounds
    if (max_line_number == 0 || cur_r < max_line_number) {
      mvwchgat(w, target_y, target_x, 1, A_REVERSE, INFO_COLOR_PAIR, NULL);
    }
  } else {
    // Selection highlight
    char* select_prefix = dumpSelection(start, tb->select_cursor);
    int sel_r = 0;
    int sel_c = 0;
    int sel_wrap_w = line_length;
    countStringFrame(select_prefix, strlen(select_prefix), &sel_r, &sel_c, &sel_wrap_w, tab_size);

    // Highlight characters between (cur_r, cur_c) and (sel_r, sel_c)
    int start_r = cur_r < sel_r ? cur_r : sel_r;
    int end_r = cur_r > sel_r ? cur_r : sel_r;
    int start_c = cur_r < sel_r ? cur_c : (cur_r > sel_r ? sel_c : (cur_c < sel_c ? cur_c : sel_c));
    int end_c = cur_r < sel_r ? sel_c : (cur_r > sel_r ? cur_c : (cur_c > sel_c ? cur_c : sel_c));

    for (int r = start_r; r <= end_r; r++) {
      if (max_line_number > 0 && r >= max_line_number) break;
      int line_start_x = (r == start_r) ? start_c : 0;
      int line_end_x = (r == end_r) ? end_c : line_length;
      int len = line_end_x - line_start_x;
      if (len <= 0) len = 1;
      mvwchgat(w, offset_y + r, offset_x + line_start_x, len, A_REVERSE, INFO_COLOR_PAIR, NULL);
    }
    free(select_prefix);
  }

  free(prefix);
  free(full_text);
}
