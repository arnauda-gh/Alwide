#include "few.h"

#include <libgen.h>
#include <ncurses.h>

#include "../../core/editor_context.h"
#include "../../data-management/file_management.h"
#include "../../environnement/constants.h"
#include "../../terminal/click_handler.h"
#include "../../terminal/key_management.h"
#include "../../utils/tools.h"
#include "edw.h"
#include "ofw.h"
#include "popups/few_action_popup.h"


void gui_initFEWContext(gui_FEW* context) {
  context->few = NULL;         // File Explorer Window
  context->refresh_few = true; // Need to reprint file explorer window

  context->few_width = 0; // File explorer width
  context->saved_few_width = FILE_EXPLORER_WIDTH;
  context->few_x_offset = 0; /* TODO unused */
  context->few_y_offset = 0; // Y Scroll state of File Explorer Window
  context->few_selected_line = -1;
}


void gui_resizeFEW(gui_Context* gui_context, int few_new_width) {
  if (few_new_width == -1) {
    few_new_width = gui_context->few_context.few_width;
  }
  few_new_width = max(0, few_new_width);
  gui_context->few_context.few_width = few_new_width;
  // Resize File Explorer Window
  delwin(gui_context->few_context.few);
  // We don't allocate a window if the width is NULL.
  if (gui_context->few_context.few_width == 0) {
    gui_context->few_context.few = NULL;
  }
  else {
    gui_context->few_context.few = newwin(0, gui_context->few_context.few_width, 0, 0);
    wbkgd(gui_context->few_context.few, COLOR_PAIR(DEFAULT_COLOR_PAIR));
  }

  // Resize Opened File Window
  gui_resizeOFW(gui_context);
  // Resize Editor Window
  gui_resizeEDW(gui_context, -1);
  gui_context->few_context.refresh_few = true;
}

void gui_switchFEW(gui_Context* gui_context) {
  if (gui_context->few_context.few == NULL) {
    // Open File Explorer Window
    gui_context->few_context.few_width = gui_context->few_context.saved_few_width;
    gui_context->few_context.few = newwin(0, gui_context->few_context.few_width, 0, 0);
    gui_context->few_context.refresh_few = true;
    wbkgd(gui_context->few_context.few, COLOR_PAIR(DEFAULT_COLOR_PAIR));
    gui_context->focused_panel = PANEL_FILE_EXPLORER;
  }
  else {
    // Close File Explorer Window
    gui_context->few_context.saved_few_width = getmaxx(gui_context->few_context.few);
    delwin(gui_context->few_context.few);
    gui_context->few_context.few = NULL;
    gui_context->few_context.few_width = 0;
    gui_context->focused_panel = PANEL_EDITOR;
  }
  // Resize Opened File Window
  gui_resizeOFW(gui_context);
  // Resize Editor Window
  gui_resizeEDW(gui_context, -1);
}


#define SELECTED_ATTRIBUTE (A_STANDOUT | A_DIM)

void internalPrintExplorerRec(ExplorerFolder* folder, WINDOW* few, int* few_x_offset, int* few_y_offset,
                              int tree_offset_rec, int* selected_line, bool is_focused) {
  // Don't print if not in window.
  if (getcury(few) + 1 >= getmaxy(few)) {
    return;
  }

  if (folder->open && folder->discovered == false) {
    discoverFolder(folder);
  }

  (*selected_line)--;
  if (*few_y_offset == 0) {
    // Print current folder name

    if (*selected_line == 0) {
      if (is_focused) {
        wattron(few, A_STANDOUT);
      }
      else {
        wattron(few, SELECTED_ATTRIBUTE);
      }
    }

    for (int i = 0; i < tree_offset_rec && i < getmaxx(few); i++) {
      printToNcursesNCharFromString(few, " ", getmaxx(few) - (getcurx(few) + 1));
    }

    // Print decoration of folder. The decoration describe if the folder is open or not.
    if (folder->open) {
      printToNcursesNCharFromString(few, "⌄", getmaxx(few) - (getcurx(few) + 1));
    }
    else {
      printToNcursesNCharFromString(few, "›", getmaxx(few) - (getcurx(few) + 1));
    }

    printToNcursesNCharFromString(few, "📁", getmaxx(few) - (getcurx(few) + 1));
    printToNcursesNCharFromString(few, basename(folder->path), getmaxx(few) - (getcurx(few) + 1));
    if (*selected_line == 0) {
      for (int j = getcurx(few) + 1; j < getmaxx(few); j++) {
        printToNcursesNCharFromString(few, " ", getmaxx(few) - (getcurx(few) + 1));
      }
      if (is_focused) {
        wattroff(few, A_STANDOUT);
      }
      else {
        wattroff(few, SELECTED_ATTRIBUTE);
      }
    }
    wprintw(few, "\n");
  }
  else {
    (*few_y_offset)--;
  }

  if (folder->open == false) {
    return;
  }

  // Print sub folders
  for (int i = 0; i < folder->folder_count; i++) {
    internalPrintExplorerRec(folder->folders + i, few, few_x_offset, few_y_offset,
                             tree_offset_rec + FILE_EXPLORER_TREE_OFFSET, selected_line, is_focused);
  }
  // Print sub files
  for (int i = 0; i < folder->file_count; i++) {
    // Don't print if not in window.
    if (getcury(few) + 1 >= getmaxy(few)) {
      return;
    }

    (*selected_line)--;
    if (*few_y_offset == 0) {
      // Print file name
      if (*selected_line == 0) {
        if (is_focused) {
          wattron(few, A_STANDOUT);
        }
        else {
          wattron(few, SELECTED_ATTRIBUTE);
        }
      }
      for (int j = 0;
           j < tree_offset_rec + FILE_EXPLORER_TREE_OFFSET + 1 /*Add one to balance with the folder decoration*/; j++) {
        printToNcursesNCharFromString(few, " ", getmaxx(few) - (getcurx(few) + 1));
      }
      printToNcursesNCharFromString(few, "📄", getmaxx(few) - (getcurx(few) + 1));
      printToNcursesNCharFromString(few, basename(folder->files[i].path), getmaxx(few) - (getcurx(few) + 1));
      if (*selected_line == 0) {
        for (int j = getcurx(few) + 1; j < getmaxx(few); j++) {
          printToNcursesNCharFromString(few, " ", getmaxx(few) - (getcurx(few) + 1));
        }
        if (is_focused) {
          wattroff(few, A_STANDOUT);
        }
        else {
          wattroff(few, SELECTED_ATTRIBUTE);
        }
      }
      wprintw(few, "\n");
    }
    else {
      (*few_y_offset)--;
    }
  }
}

void gui_repaintFEW(gui_FEW* context, ExplorerFolder* pwd, bool is_focused) {
  if (!(context->refresh_few == true && context->few_width != 0 && context->few != NULL)) {
    context->refresh_few = false;
    return;
  }
  fprintf(stderr, "print FEW\n");
  werase(context->few);
  wmove(context->few, 0, 0);

  // the internal fct need edit this var to lake them.
  // We need to make a copy of them to keep the value right in the gui_context.
  int tmp_few_x_offset = context->few_x_offset;
  int tmp_few_y_offset = context->few_y_offset;
  int tmp_few_selected_line = context->few_selected_line;
  internalPrintExplorerRec(pwd, context->few, &tmp_few_x_offset, &tmp_few_y_offset, 0, &tmp_few_selected_line,
                           is_focused);
  // Clear end of window

  for (int i = getbegy(context->few); i < getmaxy(context->few); i++) {
    mvwprintw(context->few, i, getmaxx(context->few) - 1, "│");
  }

  wnoutrefresh(context->few);
  context->refresh_few = false;
}

static int count_total_explorer_lines(ExplorerFolder* folder) {
  int count = 1; // for the folder itself
  if (!folder->open) {
    return count;
  }
  for (int i = 0; i < folder->folder_count; i++) {
    count += count_total_explorer_lines(&folder->folders[i]);
  }
  count += folder->file_count;
  return count;
}

bool gui_handleFEWKeyboardInput(void* ctx_void, int key) {
  EditorContext* ctx = (EditorContext*)ctx_void;
  if (ctx->gui_context.focused_panel != PANEL_FILE_EXPLORER) {
    return false;
  }

  gui_FEW* few = &ctx->gui_context.few_context;

  if (few->few_selected_line == -1) {
    few->few_selected_line = 1;
  }

  int total_lines = count_total_explorer_lines(&ctx->pwd);

  switch (key) {
    case H_KEY_UP:
    case 'k':
      if (few->few_selected_line > 1) {
        few->few_selected_line--;
        if (few->few_selected_line - 1 < few->few_y_offset) {
          few->few_y_offset = few->few_selected_line - 1;
        }
        gui_updateFEW(&ctx->gui_context);
      }
      return true;

    case H_KEY_DOWN:
    case 'j':
      if (few->few_selected_line < total_lines) {
        few->few_selected_line++;
        int view_height = getmaxy(few->few);
        if (few->few_selected_line - few->few_y_offset > view_height) {
          few->few_y_offset++;
        }
        gui_updateFEW(&ctx->gui_context);
      }
      return true;
    case H_KEY_LEFT:
    case 'h':
      {
        ExplorerFolder* res_folder;
        int res_index;
        if (getFileClickedFileExplorer(&ctx->pwd, few->few_selected_line - 1, 0, 0, &res_folder, &res_index)) {
          if (res_index == -1) {
            if (res_folder->open) {
              res_folder->open = false;
              gui_updateFEW(&ctx->gui_context);
            }
          }
        }
        return true;
      }

    case H_KEY_RIGHT:
    case 'l':
      {
        ExplorerFolder* res_folder;
        int res_index;
        if (getFileClickedFileExplorer(&ctx->pwd, few->few_selected_line - 1, 0, 0, &res_folder, &res_index)) {
          if (res_index == -1) {
            if (!res_folder->open) {
              res_folder->open = true;
              gui_updateFEW(&ctx->gui_context);
            }
          }
        }
        return true;
      }

    case H_KEY_ENTER:
    case ' ':
    case K_SPECIAL(0, '\r'):
      {
        ExplorerFolder* res_folder;
        int res_index;
        if (getFileClickedFileExplorer(&ctx->pwd, few->few_selected_line - 1, 0, 0, &res_folder, &res_index)) {
          if (res_index == -1) {
            res_folder->open = !res_folder->open;
          }
          else {
            openNewFile(res_folder->files[res_index].path, &ctx->files, &ctx->file_count, &ctx->current_file_index,
                        &ctx->gui_context.ofw_context.refresh_ofw, &ctx->refresh_local_vars);
            ctx->gui_context.focused_panel = PANEL_EDITOR;
          }
          gui_updateGUI(&ctx->gui_context);
        }
        return true;
      }

    // CRUD Keybinds
    case K_SPECIAL(K_MOD_CTRL, KEY_ENTER):
    case 'a':
      {
        ExplorerFolder* res_folder;
        int res_index;
        if (getFileClickedFileExplorer(&ctx->pwd, few->few_selected_line - 1, 0, 0, &res_folder, &res_index)) {
          gui_openExplorerContextPopup(ctx, few->few_selected_line - few->few_y_offset, few->few_width / 2, res_folder,
                                       res_index);
        }
        return true;
      }

    case 'r':
      {
        ExplorerFolder* res_folder;
        int res_index;
        if (getFileClickedFileExplorer(&ctx->pwd, few->few_selected_line - 1, 0, 0, &res_folder, &res_index)) {
          gui_openExplorerContextPopup(ctx, few->few_selected_line - few->few_y_offset, few->few_width / 2, res_folder,
                                       res_index);
        }
        return true;
      }

    case 'x':
    case K_SPECIAL(0, KEY_DC):
      { // delete key
        ExplorerFolder* res_folder;
        int res_index;
        if (getFileClickedFileExplorer(&ctx->pwd, few->few_selected_line - 1, 0, 0, &res_folder, &res_index)) {
          gui_openExplorerContextPopup(ctx, few->few_selected_line - few->few_y_offset, few->few_width / 2, res_folder,
                                       res_index);
        }
        return true;
      }

    case 'R':
    case K_SPECIAL(0, KEY_F(5)):
      reloadFolder(&ctx->pwd);
      gui_updateFEW(&ctx->gui_context);
      return true;

    case H_KEY_ESCAPE:
      ctx->gui_context.focused_panel = PANEL_EDITOR;
      gui_updateGUI(&ctx->gui_context);
      return true;

    case H_KEY_MOUSE:
      return false;
  }

  return true;
}
