#include "auto_pairs.h"
#include <string.h>
#include "../../advanced/shared.h"
#include "../../data-management/utf_8_extractor.h"
#include "../../terminal/term_handler.h"

bool ilj_handleAutoPairs(FileContainer* fc, Char_U8 input, History** history_p,
                         PayloadStateChange payload_state_change) {
  if (!fc->feature || fc->feature->pairs_count == 0) {
    return false;
  }

  // 1. Check for overtyping a closing character
  Char_U8 current_ch = {0};
  if (fc->cursor.line_id.relative_column < fc->cursor.line_id.line->element_number) {
    current_ch = fc->cursor.line_id.line->ch[fc->cursor.line_id.relative_column];
  }
  else if (fc->cursor.line_id.line->next != NULL && fc->cursor.line_id.line->next->element_number > 0) {
    current_ch = fc->cursor.line_id.line->next->ch[0];
  }

  for (int i = 0; i < fc->feature->pairs_count; i++) {
    LF_Pair* pair = &fc->feature->pairs[i];
    Char_U8 close_u8 = readChar_U8FromCharArray(pair->close);
    if (areChar_U8Equals(input, close_u8) && areChar_U8Equals(input, current_ch)) {
      fc->cursor = moveRight(fc->cursor);
      return true;
    }
  }

  // 2. Check for inserting an opening character
  for (int i = 0; i < fc->feature->pairs_count; i++) {
    LF_Pair* pair = &fc->feature->pairs[i];
    Char_U8 open_u8 = readChar_U8FromCharArray(pair->open);

    if (areChar_U8Equals(input, open_u8)) {
      char combined[9] = {0};
      strcat(combined, pair->open);
      strcat(combined, pair->close);

      fc->cursor =
        insertCharArrayAtCursorWithState(history_p, fc->cursor, combined, &payload_state_change, LF_tab(fc->feature));

      // Move cursor back one position to be between the pairs
      fc->cursor = moveLeft(fc->cursor);
      return true;
    }
  }

  return false;
}

bool ilj_handleAutoPairDelete(FileContainer* fc, History** history_p, PayloadStateChange payload_state_change) {
  if (!fc->feature || fc->feature->pairs_count == 0) {
    return false;
  }

  // Get the character to the left (already before the cursor).
  Char_U8 left_ch = getCharAtCursor(fc->cursor);

  // Get the character to the right (move right then get the character before that new position).
  Cursor right_cur = moveRight(fc->cursor);
  if (cursor_eq(right_cur, fc->cursor)) {
    return false; // At the end of the file.
  }
  Char_U8 right_ch = getCharAtCursor(right_cur);

  // Check if left_ch / right_ch match any configured open/close pair.
  for (int i = 0; i < fc->feature->pairs_count; i++) {
    LF_Pair* pair = &fc->feature->pairs[i];
    Char_U8 open_u8 = readChar_U8FromCharArray(pair->open);
    Char_U8 close_u8 = readChar_U8FromCharArray(pair->close);

    // Skip self-closing pairs like '"' / '"' to avoid ambiguity on deletion.
    if (areChar_U8Equals(open_u8, close_u8)) {
      continue;
    }

    if (areChar_U8Equals(left_ch, open_u8) && areChar_U8Equals(right_ch, close_u8)) {
      // Cursor is exactly between a matched pair, e.g. '(|)'.
      // Delete both the character to the left and the character to the right.
      Cursor select_cursor = moveRight(fc->cursor);
      fc->cursor = moveLeft(fc->cursor);

      deleteSelectionWithState(history_p, &fc->cursor, &select_cursor, &payload_state_change);

      return true;
    }
  }

  return false;
}
