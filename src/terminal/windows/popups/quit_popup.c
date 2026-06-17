#include "quit_popup.h"
#include <stdlib.h>
#include <string.h>

#include "../../../data-management/file_management.h"
#include "../../../data-management/file_structure.h"
#include "../../../data-management/state_control.h"
#include "../../../environnement/constants.h"
#include "../../../io-management/viewport_history.h"
#include "../../key_management.h"
#include "../../term_handler.h"
#include "../tpw.h"

static void paint_quit_popup(gui_TPW* popup, void* payload) {
  WINDOW* w = popup->tpw;
  int width = popup->width;

  werase(w);
  box(w, 0, 0);

  // Centered Title
  wattron(w, A_BOLD | COLOR_PAIR(WARNING_COLOR_PAIR));
  mvwprintw(w, 0, (width - 20) / 2, " [ Unsaved Changes ] ");
  wattroff(w, A_BOLD | COLOR_PAIR(WARNING_COLOR_PAIR));

  // Message
  mvwprintw(w, 2, (width - 37) / 2, "You have at least one file not saved.");

  // Footer Options
  wattron(w, A_BOLD | COLOR_PAIR(INFO_COLOR_PAIR));
  mvwprintw(w, 4, 4, "[Ctrl + S] Save and quit");
  wattroff(w, A_BOLD | COLOR_PAIR(INFO_COLOR_PAIR));

  wattron(w, A_BOLD | COLOR_PAIR(ERROR_COLOR_PAIR));
  mvwprintw(w, 4, 30, "[Ctrl + Q] Quit without saving");
  wattroff(w, A_BOLD | COLOR_PAIR(ERROR_COLOR_PAIR));
}

static bool input_quit_popup(gui_TPW* popup, int key, MEVENT* m_event, void* payload) {
  EditorContext* ctx = (EditorContext*)payload;

  // q : quit
  if (key == 'q') {
    gui_closeTPW(&ctx->gui_context, popup);
    return true;
  }

  // Ctrl+S: Save and quit
  if (key == K_SPECIAL(K_MOD_CTRL, 's')) {
    // Save all edited files
    for (int i = 0; i < ctx->file_count; i++) {
      FileContainer* fc = &ctx->files[i];
      if (isFileEdited(fc) && fc->io_file.status != NONE) {
        saveFileContainer(fc);
        setlastFilePosition(fc->io_file.path_abs, cursor_row(fc->cursor), cursor_col(fc->cursor), fc->screen_x, fc->screen_y);
        saveCurrentStateControl(*fc->history_root, fc->history_frame, fc->io_file.path_abs);
      }
    }
    ctx->force_quit = true;
    gui_closeTPW(&ctx->gui_context, popup);
    gui_updateGUI(&ctx->gui_context);
    return true;
  }

  // Ctrl+Q: Quit without saving
  if (key == K_SPECIAL(K_MOD_CTRL, 'q')) {
    ctx->force_quit = true;
    gui_closeTPW(&ctx->gui_context, popup);
    gui_updateGUI(&ctx->gui_context);
    return true;
  }

  // Consume any other key inputs so they do not fall back into the editor
  return true;
}

static void destroy_quit_popup(gui_TPW* popup, void* payload) {
  EditorContext* ctx = (EditorContext*)payload;
  ctx->quit_popup_active = false;
}

void gui_openQuitPopup(EditorContext* ctx) {
  if (ctx->quit_popup_active) {
    return;
  }

  // Position at center: height=6, width=62
  gui_TPW* popup = gui_showTPWPositioned(&ctx->gui_context, 6, 62, GUI_TPW_POS_CENTER,
                                                     paint_quit_popup, input_quit_popup,
                                                     destroy_quit_popup, ctx);
  if (popup) {
    ctx->quit_popup_active = true;
    gui_updateGUI(&ctx->gui_context);
  }
}

bool gui_isQuitPopupActive(EditorContext* ctx) {
  return ctx->quit_popup_active;
}
