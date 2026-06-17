#include "click_handler.h"

#include <assert.h>
#include <libgen.h>
#include <string.h>
#include <time.h>

#include "../advanced/lsp/lsp-features/lsp_code_action.h"
#include "../core/features/navigation_history.h"
#include "../data-management/file_management.h"
#include "../environnement/constants.h"
#include "../environnement/global_variables.h"
#include "key_management.h"
#include "term_handler.h"
#include "windows/edw.h"
#include "windows/few.h"
#include "windows/popups/explorer_context_popup.h"
#include "windows/popups/language_popup.h"
#include "windows/pow.h"
#include "windows/tpw.h"

static int find_clicked_tab_index(gui_Context* gui, FileContainer* files, int file_count, int current_file,
                                  const MEVENT* m);

/* -------------------------------------------------------------------------- */
/*                                Mouse Helpers                               */
/* -------------------------------------------------------------------------- */

static inline bool is_left_click_pressed(const MEVENT* m) { return m->bstate & BUTTON1_PRESSED; }
static inline bool is_left_click_released(const MEVENT* m) { return m->bstate & BUTTON1_RELEASED; }
static inline bool is_left_click(const MEVENT* m) { return m->bstate & BUTTON1_CLICKED; }
static inline bool is_left_double_click(const MEVENT* m) { return m->bstate & BUTTON1_DOUBLE_CLICKED; }
static inline bool is_left_click_action(const MEVENT* m) { return is_left_click_pressed(m) || is_left_click(m); }

static inline bool is_right_click_pressed(const MEVENT* m) { return m->bstate & BUTTON3_PRESSED; }
static inline bool is_right_click_released(const MEVENT* m) { return m->bstate & BUTTON3_RELEASED; }
static inline bool is_right_click(const MEVENT* m) { return m->bstate & BUTTON3_CLICKED; }
static inline bool is_right_double_click(const MEVENT* m) { return m->bstate & BUTTON3_DOUBLE_CLICKED; }
static inline bool is_right_click_action(const MEVENT* m) { return is_right_click_pressed(m) || is_right_click(m); }

static inline bool is_scroll_up(const MEVENT* m) { return m->bstate & BUTTON4_PRESSED; }
static inline bool is_scroll_down(const MEVENT* m) { return m->bstate & BUTTON5_PRESSED; }
static inline bool is_scroll_left(const MEVENT* m) { return m->bstate & BUTTON6_PRESSED; }
static inline bool is_scroll_right(const MEVENT* m) { return m->bstate & BUTTON7_PRESSED; }

static inline bool is_shift_active(const MEVENT* m) { return m->bstate & BUTTON_SHIFT; }
static inline bool is_ctrl_active(const MEVENT* m) { return m->bstate & BUTTON_CTRL; }

/* -------------------------------------------------------------------------- */
/*                               Internal Utils                               */
/* -------------------------------------------------------------------------- */

bool isClickInsideWindow(WINDOW* w, MEVENT* m_event) {
  if (w == NULL) {
    return false;
  }
  int beg_x, beg_y, max_x, max_y;
  getbegyx(w, beg_y, beg_x);
  getmaxyx(w, max_y, max_x);

  return (m_event->x >= beg_x && m_event->x < beg_x + max_x && m_event->y >= beg_y && m_event->y < beg_y + max_y);
}

static Cursor calculate_cursor_from_click(const Cursor* current, const MEVENT* m, int screen_x, int screen_y,
                                          int offset_x, int offset_y, int tab_size) {
  FileIdentifier file_id = tryToReachAbsRow(current->file_id, screen_y + m->y - offset_y);
  LineIdentifier line_id = getLineIdForScreenX(moduloLineIdentifierR(getLineForFileIdentifier(file_id), 0), screen_x,
                                               m->x - offset_x, tab_size);

  return cursorOf(file_id, line_id);
}

/* -------------------------------------------------------------------------- */
/*                               Main Handlers                                */
/* -------------------------------------------------------------------------- */

static void handle_file_explorer_interaction(EditorContext* ctx) {
  gui_Context* gui = &ctx->gui_context;

  if (is_left_click_pressed(&ctx->m_event) || is_right_click_pressed(&ctx->m_event)) {
    gui->focus_w = gui->few_context.few;
    gui->focused_panel = PANEL_FILE_EXPLORER;
  }

  handleFileExplorerClick(ctx);
}

static void handle_opened_files_interaction(EditorContext* ctx) {
  gui_Context* gui = &ctx->gui_context;

  if (is_left_click_pressed(&ctx->m_event)) {
    gui->focus_w = gui->ofw_context.ofw;
    gui->focused_panel = PANEL_EDITOR;
  }

  handleOpenedFileClick(gui, ctx->files, &ctx->file_count, &ctx->current_file_index, ctx->m_event,
                        &ctx->refresh_local_vars, ctx->mouse_drag);
}

static void handle_editor_interaction(EditorContext* ctx) {
  gui_Context* gui = &ctx->gui_context;
  FileContainer* fc = &ctx->files[ctx->current_file_index];

  // Main editor window interaction
  if (is_left_click_pressed(&ctx->m_event)) {
    if (gui->focused_panel != PANEL_EDITOR) {
      gui_updateFEW(&ctx->gui_context);
    }
    gui->focus_w = gui->edw_context.ftw;
    gui->focused_panel = PANEL_EDITOR;
  }

  // Check status bar first
  if (gui->edw_context.show_sbw && isClickInsideWindow(gui->edw_context.sbw, &ctx->m_event)) {
    if (is_left_click_pressed(&ctx->m_event) || is_left_click(&ctx->m_event)) {
      ctx->mouse_drag = false;
      gui->focus_w = NULL;
      gui_openLanguageSelectPopup(ctx);
      return;
    }
  }


  handleEditorClick(gui, &fc->cursor, &fc->select_cursor, &fc->desired_column, &fc->screen_x, &fc->screen_y,
                    &ctx->m_event, ctx->mouse_drag, fc, &ctx->highlight_descriptor);
}

void handleClick(EditorContext* ctx) {
  if (is_left_click_pressed(&ctx->m_event) && !ctx->mouse_drag) {
    ctx->mouse_drag = true;
  }

  gui_Context* gui = &ctx->gui_context;
  int edw_start_x = getbegx(gui->edw_context.lnw);

  // Dispatch based on click location or current focus
  bool click_in_explorer = (ctx->m_event.x < edw_start_x && gui->focus_w == NULL) ||
                           (gui->few_context.few != NULL && gui->focus_w == gui->few_context.few);

  bool click_in_tabs = (ctx->m_event.y < gui->ofw_context.ofw_height && gui->focus_w == NULL) ||
                       (gui->ofw_context.ofw != NULL && gui->focus_w == gui->ofw_context.ofw);

  if (click_in_explorer) {
    handle_file_explorer_interaction(ctx);
  }
  else if (click_in_tabs) {
    handle_opened_files_interaction(ctx);
  }
  else {
    handle_editor_interaction(ctx);
  }

  // Cleanup drag state
  if (is_left_click_released(&ctx->m_event) || is_left_click(&ctx->m_event)) {
    gui->focus_w = NULL;
    ctx->mouse_drag = false;
  }
}

/* -------------------------------------------------------------------------- */
/*                               Editor Handlers                              */
/* -------------------------------------------------------------------------- */

static void handle_editor_cursor_action(gui_Context* gui_context, Cursor* cursor, Cursor* select_cursor,
                                        int* desired_column, int screen_x, int screen_y, const MEVENT* m_event,
                                        bool mouse_drag, int tab_size) {
  int off_x = getbegx(gui_context->edw_context.ftw);
  int off_y = getbegy(gui_context->edw_context.ftw);

  if (mouse_drag) {
    Cursor target = calculate_cursor_from_click(cursor, m_event, screen_x, screen_y, off_x, off_y, tab_size);

    if (!cursor_is_disabled(*select_cursor)) {
      *cursor = target;
    }
    else if (cursor_ne(*cursor, target)) {
      *select_cursor = *cursor;
      *cursor = target;
    }
    setDesiredColumn(*cursor, desired_column);
    gui_closeEDWPopup(gui_context);
  }

  if (is_left_click_pressed(m_event)) {
    setSelectCursorOff(cursor, select_cursor, SELECT_OFF_RIGHT);
    *cursor = calculate_cursor_from_click(cursor, m_event, screen_x, screen_y, off_x, off_y, tab_size);
    setDesiredColumn(*cursor, desired_column);
    gui_closeEDWPopup(gui_context);
  }

  if (is_left_double_click(m_event)) {
    selectWord(cursor, select_cursor);
    gui_closeEDWPopup(gui_context);
  }
}

static void handle_editor_scrolling(gui_Context* gui, int* screen_x, int* screen_y, const MEVENT* m,
                                    const Cursor* cursor) {
  bool shifted = is_shift_active(m);

  if (is_scroll_up(m) || is_scroll_left(m)) {
    if (is_scroll_left(m) || shifted) {
      *screen_x = (*screen_x > SCROLL_SPEED) ? *screen_x - SCROLL_SPEED : 1;
    }
    else {
      *screen_y = (*screen_y > SCROLL_SPEED) ? *screen_y - SCROLL_SPEED : 1;
    }
    gui_updateEDW(gui);
  }
  else if (is_scroll_down(m) || is_scroll_right(m)) {
    if (is_scroll_right(m) || shifted) {
      *screen_x += SCROLL_SPEED;
    }
    else {
      *screen_y += SCROLL_SPEED;
      FileIdentifier last = tryToReachAbsRow(cursor->file_id, *screen_y + 2);
      if (*screen_y + 2 != last.absolute_row) {
        *screen_y = (last.absolute_row > 2) ? last.absolute_row - 2 : 1;
      }
    }
    gui_updateEDW(gui);
  }
}

static bool handle_editor_gutter_interaction(gui_Context* gui, int screen_y, const MEVENT* m, FileContainer* file,
                                             WindowHighlightDescriptor* whd) {
  LSP_Diagnostic* diag = NULL;
  int row = screen_y + m->y - getbegy(gui->edw_context.lnw);
  LineMarker marker = gui_getMarkerForCurrentLine(row, whd, 0, (void**)&diag);

  bool is_lsp_marker =
    (marker == LSP_ERROR || marker == LSP_HINT || marker == LSP_INFORMATION || marker == LSP_WARNING);

  if (is_lsp_marker && diag != NULL) {
    if (is_left_click(m)) {
      if (gui->edw_context.pow_owner == COMPLETION) {
        LSP_destroyCompletionList(&file->lsp_datas.computed->completions);
        gui_closeEDWPopup(gui);
      }
      file->cursor = LSP_tryToReachCursorForLSPPosition(getLSPServerForLanguage(&lsp_servers, file->lsp_datas.lang_id),
                                                        file->cursor, diag->range.pos1);
      askCodeAction(file, &file->cursor);
    }
    else if (gui->edw_context.pow_owner != COMPLETION) {
      int y_pos = m->y - getbegy(gui->edw_context.lnw) + 1;
      gui_showDiagnostic(gui, y_pos, getbegy(gui->edw_context.ftw), diag, LF_tab_size(file->feature));
    }
  }

  return is_lsp_marker;
}

static void handle_editor_content_interaction(gui_Context* gui, int screen_x, int screen_y, const MEVENT* m,
                                              FileContainer* file) {
  int off_x = getbegx(gui->edw_context.ftw);
  int off_y = getbegy(gui->edw_context.ftw);
  int tab_size = LF_tab_size(file->feature);

  Cursor hover_cursor = calculate_cursor_from_click(&file->cursor, m, screen_x, screen_y, off_x, off_y, tab_size);
  gui->edw_context.lastMousePosition = cursor_to_desc(hover_cursor);

  if (!file->lsp_datas.is_enable) {
    return;
  }

  LSP_Server* lsp = getLSPServerForLanguage(&lsp_servers, file->lsp_datas.lang_id);

  if (is_ctrl_active(m)) {
    // Ctrl + Click for Goto Definition
    if (is_left_click(m)) {
      LSP_requestGoto(lsp, file->io_file.path_abs, LSP_pos_from_cursor(lsp, hover_cursor), LSP_GOTO_DEFINITION);
    }

    // Hover information
    if (gui->edw_context.pow_owner != GOTO_CHOICE) {
      LSP_Hover* hover = &file->lsp_datas.computed->hover;
      bool already_hovering = false;

      if (hover->is_range) {
        Cursor c1 = LSP_tryToReachCursorForLSPPosition(lsp, hover_cursor, hover->range.pos1);
        Cursor c2 = LSP_tryToReachCursorForLSPPosition(lsp, hover_cursor, hover->range.pos2);
        already_hovering = cursor_desc_is_between(cursor_to_desc(hover_cursor), cursor_to_desc(c1), cursor_to_desc(c2));
      }

      if (!already_hovering) {
        LSP_requestHover(lsp, file->io_file.path_abs, LSP_pos_from_cursor(lsp, hover_cursor));
      }
      else if (hover->size != 0) {
        ViewPort vp = {.gui = gui, .screen_x = &file->screen_x, .screen_y = &file->screen_y};
        gui_resumeHoverInformation(&file->cursor, &vp, file, hover, tab_size);
      }
    }
  }
}

void handleEditorClick(gui_Context* gui, Cursor* cursor, Cursor* select_cursor, int* desired_column, int* screen_x,
                       int* screen_y, MEVENT* m, bool mouse_drag, FileContainer* file, WindowHighlightDescriptor* whd) {
  int off_y = getbegy(gui->edw_context.ftw);
  if (m->y < off_y) {
    m->y = off_y;
  }

  // Interaction chain (matches original else-if logic)
  if (isClickInsideWindow(gui->edw_context.lnw, m)) {
    if (handle_editor_gutter_interaction(gui, *screen_y, m, file, whd)) {
      return;
    }
  }

  int tab_size = LF_tab_size(file->feature);

  handle_editor_cursor_action(gui, cursor, select_cursor, desired_column, *screen_x, *screen_y, m, mouse_drag,
                              tab_size);
  handle_editor_scrolling(gui, screen_x, screen_y, m, cursor);


  if (isClickInsideWindow(gui->edw_context.ftw, m)) {
    handle_editor_content_interaction(gui, *screen_x, *screen_y, m, file);
  }
  else if (gui->edw_context.show_pow == true &&
           (gui->edw_context.pow_owner == DIAGNOSTICS || gui->edw_context.pow_owner == HOVER_DIAGNOSTICS)) {
    // gui_closePopup(gui);
  }
}

/* -------------------------------------------------------------------------- */
/*                                Tab Handlers                                */
/* -------------------------------------------------------------------------- */

static int find_clicked_tab_index(gui_Context* gui, FileContainer* files, int file_count, int current_file,
                                  const MEVENT* m) {
  gui_OFW* ofw = &gui->ofw_context;
  int x_cursor = getbegx(ofw->ofw);

  if (ofw->current_file_offset != 0) {
    x_cursor += strlen("< | ");
    if (m->x < x_cursor) {
      ofw->current_file_offset--;
      ofw->refresh_ofw = true;
      return -1;
    }
  }

  for (int i = ofw->current_file_offset; i < file_count; i++) {
    const char* name = basename(files[i].io_file.path_args);
    int name_len = strlen(name);
    int sep_len = strlen(FILE_NAME_SEPARATOR);

    // Check if we hit the right boundary
    if (x_cursor + name_len + sep_len > COLS && m->x > COLS - 4) {
      ofw->current_file_offset++;
      ofw->refresh_ofw = true;
      return -1;
    }

    if (m->x < x_cursor + name_len + (sep_len / 2)) {
      return (current_file == i) ? -1 : i;
    }

    x_cursor += name_len + sep_len;
  }

  return -1;
}

void handleOpenedFileClick(gui_Context* gui, FileContainer* files, int* file_count, int* current_file, MEVENT m,
                           bool* refresh_local_vars, bool mouse_drag) {
  gui_OFW* ofw = &gui->ofw_context;

  if (is_scroll_up(&m) || is_scroll_left(&m)) {
    if (ofw->current_file_offset > 0) {
      ofw->current_file_offset--;
      gui_updateOFW(gui);
    }
    return;
  }

  if (is_scroll_down(&m) || is_scroll_right(&m)) {
    if (ofw->current_file_offset < *file_count - 1) {
      ofw->current_file_offset++;
      gui_updateOFW(gui);
    }
    return;
  }

  if (is_left_click_pressed(&m)) {
    int clicked = find_clicked_tab_index(gui, files, *file_count, *current_file, &m);
    if (clicked != -1) {
      *current_file = clicked;
      *refresh_local_vars = true;
      gui_updateOFW(gui);
    }
    return;
  }

  // Handle tab reordering via drag
  if (m.bstate == NO_EVENT_MOUSE && mouse_drag) {
    int target = find_clicked_tab_index(gui, files, *file_count, *current_file, &m);
    if (target != -1 && target != *current_file) {
      FileContainer tmp = files[target];
      files[target] = files[*current_file];
      files[*current_file] = tmp;

      *current_file = target;
      *refresh_local_vars = true;
      gui_updateOFW(gui);
    }
  }
}

/* -------------------------------------------------------------------------- */
/*                           File Explorer Handlers                           */
/* -------------------------------------------------------------------------- */

static bool find_explorer_item(ExplorerFolder* folder, int* y_rel, ExplorerFolder** res_folder, int* file_idx) {
  if (*y_rel == 0) {
    *res_folder = folder;
    *file_idx = -1;
    return true;
  }
  (*y_rel)--;

  if (!folder->open) {
    return false;
  }

  for (int i = 0; i < folder->folder_count; i++) {
    if (find_explorer_item(&folder->folders[i], y_rel, res_folder, file_idx)) {
      return true;
    }
  }

  for (int i = 0; i < folder->file_count; i++) {
    if (*y_rel == 0) {
      *res_folder = folder;
      *file_idx = i;
      return true;
    }
    (*y_rel)--;
  }

  return false;
}

bool getFileClickedFileExplorer(ExplorerFolder* pwd, int y_click, int off_x, int off_y, ExplorerFolder** res_folder,
                                int* file_idx) {
  int target_y = y_click + off_y;
  return find_explorer_item(pwd, &target_y, res_folder, file_idx);
}

void handleFileExplorerClick(EditorContext* ctx) {
  gui_FEW* few = &ctx->gui_context.few_context;
  MEVENT m = ctx->m_event;
  gui_Context* gui = &ctx->gui_context;
  ExplorerFolder* pwd = &ctx->pwd;

  // Scrolling and Resizing
  if (is_scroll_up(&m) || is_scroll_left(&m)) {
    if (is_scroll_left(&m) || is_shift_active(&m)) {
      gui_resizeFEW(gui, few->few_width + 1);
    }
    else {
      few->few_y_offset = (few->few_y_offset > SCROLL_SPEED) ? few->few_y_offset - SCROLL_SPEED : 0;
    }
    gui_updateFEW(gui);
    return;
  }

  if (is_scroll_down(&m) || is_scroll_right(&m)) {
    if (is_scroll_right(&m) || is_shift_active(&m)) {
      if (few->few_width < COLS - 8) {
        gui_resizeFEW(gui, few->few_width - 1);
      }
    }
    else {
      few->few_y_offset += SCROLL_SPEED;
    }
    gui_updateFEW(gui);
    return;
  }

  if (is_right_click_pressed(&m)) {
    few->few_selected_line = few->few_y_offset + m.y + 1;
    gui_updateFEW(gui);
    ExplorerFolder* res_folder;
    int res_index;
    if (getFileClickedFileExplorer(pwd, m.y, gui->few_context.few_x_offset, gui->few_context.few_y_offset, &res_folder,
                                   &res_index)) {
      gui_openExplorerContextPopup(ctx, ctx->m_event.y + 1, ctx->m_event.x, res_folder, res_index);
    }
    return;
  }

  // Selection
  if (is_left_click_pressed(&m)) {
    few->few_selected_line = few->few_y_offset + m.y + 1;
    gui_updateFEW(gui);
  }

  // Activation (Open file / Toggle folder)
  if (is_left_double_click(&m)) {
    ExplorerFolder* res_folder;
    int res_index;

    if (getFileClickedFileExplorer(pwd, m.y, few->few_x_offset, few->few_y_offset, &res_folder, &res_index)) {
      if (res_index == -1) {
        res_folder->open = !res_folder->open;
      }
      else {
        openNewFile(res_folder->files[res_index].path, &ctx->files, &ctx->file_count, &ctx->current_file_index,
                    &gui->ofw_context.refresh_ofw, &ctx->refresh_local_vars);
      }
      gui_updateFEW(gui);
    }
  }
}

/* -------------------------------------------------------------------------- */
/*                               Popup Dispatch                               */
/* -------------------------------------------------------------------------- */

bool dispatchInputToTPW(EditorContext* ctx, int key) {
  gui_TPW* popup = ctx->gui_context.toplevel_popups;
  while (popup != NULL) {
    if (popup->visible && popup->on_input) {
      bool in_window = (key == H_KEY_MOUSE) ? isClickInsideWindow(popup->tpw, &ctx->m_event) : false;

      if (key != H_KEY_MOUSE || in_window) {
        // if in window or key input

        // default esc sequence to close popup.
        if (popup->closable && (key == H_KEY_ESCAPE || key == K_SPECIAL(K_MOD_CTRL, '['))) {
          gui_closeTPW(&ctx->gui_context, popup);
          return true;
        }

        // dispatch to the custom input function.
        if (popup->on_input(popup, key, &ctx->m_event, popup->payload)) {
          return true;
        }
      }
      else if (key == H_KEY_MOUSE && !in_window) {

        // if mouse event but not in window close if needed.
        if (popup->closable && is_left_click_pressed(&ctx->m_event)) {
          gui_closeTPW(&ctx->gui_context, popup);
          // we don't consume the mouse event.
          return false;
        }
      }

      if (popup->strong_focus) {
        return true;
      }
    }
    popup = popup->next;
  }
  return false;
}
