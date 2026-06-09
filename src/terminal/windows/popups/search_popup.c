#include "search_popup.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "../../../advanced/intelligence/search.h"
#include "../../../data-management/file_management.h"
#include "../../../environnement/constants.h"
#include "../../../utils/clipboard_manager.h"
#include "../../key_management.h"
#include "../../term_handler.h"
#include "../tpw.h"
#include "../widgets/text_box.h"

typedef struct {
  TextBuffer query_buffer;
  bool case_sensitive;
  bool wrap;
  Cursor initial_cursor;
  bool last_search_failed;
  EditorContext* ctx;
} SearchPopupContext;

static void paint_search_popup(gui_TPW* popup, void* payload) {
  SearchPopupContext* state = (SearchPopupContext*)payload;
  WINDOW* w = popup->tpw;
  int width = popup->width;

  werase(w);
  box(w, 0, 0);

  // Centered Premium Title
  wattron(w, A_BOLD | COLOR_PAIR(INFO_COLOR_PAIR));
  mvwprintw(w, 0, (width - 16) / 2, " [ Find Text ] ");
  wattroff(w, A_BOLD | COLOR_PAIR(INFO_COLOR_PAIR));

  // Options status
  mvwprintw(w, 1, 2, "[Aa] Case-sensitive: %s (Ctrl+G to toggle)", state->case_sensitive ? "ON" : "OFF");

  // Query Field
  mvwprintw(w, 2, 2, "Search: ");
  renderTextBuffer(w, &state->query_buffer, 10, 2, width - 12, 1);

  // Status message / Instruction footer
  if (state->last_search_failed) {
    wattron(w, COLOR_PAIR(ERROR_COLOR_PAIR) | A_BOLD);
    mvwprintw(w, 3, 2, "Status: No matches found!");
    wattroff(w, COLOR_PAIR(ERROR_COLOR_PAIR) | A_BOLD);
  }
  else {
    mvwprintw(w, 3, 2, "ENTER: Next | Ctrl+P: Prev | ESC: Close");
  }
}

static void perform_incremental_search(SearchPopupContext* state) {
  FileContainer* fc = &state->ctx->files[state->ctx->current_file_index];

  Cursor start = tryToReachAbsPosition(state->query_buffer.cursor, 1, 0);
  Cursor end = tryToReachAbsPosition(state->query_buffer.cursor, INT_MAX, INT_MAX);
  char* query_str = dumpSelection(start, end);

  if (strlen(query_str) == 0) {
    fc->cursor = state->initial_cursor;
    fc->select_cursor = cursor_disable(fc->select_cursor);
    state->last_search_failed = false;
    moveScreenToMatchCursor(&state->ctx->gui_context, fc->cursor, &fc->screen_x, &fc->screen_y,
                            LF_tab_size(fc->feature));
    free(query_str);
    return;
  }

  Cursor orig_cursor = fc->cursor;
  fc->cursor = state->initial_cursor;

  Cursor start_cur, end_cur;
  if (ilj_findNext(fc, query_str, state->case_sensitive, true, &start_cur, &end_cur)) {
    fc->select_cursor = start_cur;
    fc->cursor = end_cur;
    state->last_search_failed = false;
    moveScreenToMatchCursor(&state->ctx->gui_context, fc->cursor, &fc->screen_x, &fc->screen_y,
                            LF_tab_size(fc->feature));
  }
  else {
    fc->cursor = orig_cursor;
    state->last_search_failed = true;
  }
  free(query_str);
}

static bool input_search_popup(gui_TPW* popup, int key, MEVENT* m_event, void* payload) {
  SearchPopupContext* state = (SearchPopupContext*)payload;
  FileContainer* fc = &state->ctx->files[state->ctx->current_file_index];

  // 1. ESC: Exit search
  if (key == H_KEY_ESCAPE || key == K_SPECIAL(K_MOD_CTRL, '[')) {
    EditorContext* ctx = state->ctx;
    gui_destroyToplevelPopup(&ctx->gui_context, popup);
    gui_updateGUI(&ctx->gui_context);
    return true;
  }

  // 2. Ctrl+G: Toggle Case Sensitivity
  if (key == K_SPECIAL(K_MOD_CTRL, 'g')) {
    state->case_sensitive = !state->case_sensitive;
    perform_incremental_search(state);
    gui_updateGUI(&state->ctx->gui_context);
    return true;
  }

  // 3. ENTER / Find Next
  if (key == H_KEY_ENTER) {
    Cursor start = tryToReachAbsPosition(state->query_buffer.cursor, 1, 0);
    Cursor end = tryToReachAbsPosition(state->query_buffer.cursor, INT_MAX, INT_MAX);
    char* query_str = dumpSelection(start, end);
    if (strlen(query_str) > 0) {
      Cursor start_cur, end_cur;
      if (ilj_findNext(fc, query_str, state->case_sensitive, state->wrap, &start_cur, &end_cur)) {
        fc->select_cursor = start_cur;
        fc->cursor = end_cur;
        state->last_search_failed = false;
        moveScreenToMatchCursor(&state->ctx->gui_context, fc->cursor, &fc->screen_x, &fc->screen_y,
                                LF_tab_size(fc->feature));
      }
      else {
        state->last_search_failed = true;
      }
    }
    free(query_str);
    gui_updateGUI(&state->ctx->gui_context);
    return true;
  }

  // 4. Ctrl+P: Find Prev
  if (key == K_SPECIAL(K_MOD_CTRL, 'p')) {
    Cursor start = tryToReachAbsPosition(state->query_buffer.cursor, 1, 0);
    Cursor end = tryToReachAbsPosition(state->query_buffer.cursor, INT_MAX, INT_MAX);
    char* query_str = dumpSelection(start, end);
    if (strlen(query_str) > 0) {
      Cursor start_cur, end_cur;
      if (ilj_findPrev(fc, query_str, state->case_sensitive, state->wrap, &start_cur, &end_cur)) {
        fc->select_cursor = start_cur;
        fc->cursor = end_cur;
        state->last_search_failed = false;
        moveScreenToMatchCursor(&state->ctx->gui_context, fc->cursor, &fc->screen_x, &fc->screen_y,
                                LF_tab_size(fc->feature));
      }
      else {
        state->last_search_failed = true;
      }
    }
    free(query_str);
    gui_updateGUI(&state->ctx->gui_context);
    return true;
  }

  // 5. Delegate text editing, navigation, selection, copy/cut/paste, and undo/redo
  if (tb_handleKey(&state->query_buffer, key, NULL)) {
    perform_incremental_search(state);
    gui_updateGUI(&state->ctx->gui_context);
    return true;
  }

  // Consume any other key inputs so they do not fall back into the editor
  return true;
}

static void destroy_search_popup(gui_TPW* popup, void* payload) {
  SearchPopupContext* state = (SearchPopupContext*)payload;
  if (state) {
    destroyTextBuffer(&state->query_buffer);
    free(state);
  }
}

void gui_openSearchPopup(EditorContext* ctx, char* query) {
  SearchPopupContext* state = malloc(sizeof(SearchPopupContext));
  if (!state) {
    return;
  }

  initTextBuffer(&state->query_buffer, &default_feature);

  if (query != NULL && strlen(query) > 0) {
    state->query_buffer.cursor =
      insertCharArrayAtCursor(state->query_buffer.cursor, query, &state->query_buffer.feature->tabulation);
    setDesiredColumn(state->query_buffer.cursor, &state->query_buffer.desired_column);
  }

  state->case_sensitive = false;
  state->wrap = true;
  state->ctx = ctx;
  state->last_search_failed = false;

  FileContainer* fc = &ctx->files[ctx->current_file_index];
  state->initial_cursor = fc->cursor;

  // Position at the bottom-right corner: height=5, width=50
  gui_createToplevelPopupPositioned(&ctx->gui_context, 5, 50, GUI_TPW_POS_BOTTOM_RIGHT, paint_search_popup,
                                    input_search_popup, destroy_search_popup, state);
  gui_updateGUI(&ctx->gui_context);
}
