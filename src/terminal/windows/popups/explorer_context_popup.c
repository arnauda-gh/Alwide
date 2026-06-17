#include "explorer_context_popup.h"
#include <dirent.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../../data-management/file_management.h"
#include "../../../environnement/constants.h"
#include "../../../environnement/global_variables.h"
#include "../../../utils/logger.h"
#include "../../click_handler.h"
#include "../../key_management.h"
#include "../../term_handler.h"
#include "../few.h"
#include "../tpw.h"
#include "../widgets/text_box.h"

// Types of operations
typedef enum { EXPLORER_ACTION_NEW_FILE, EXPLORER_ACTION_NEW_FOLDER, EXPLORER_ACTION_RENAME } ExplorerActionType;

// Context Menu State
typedef struct {
  EditorContext* ctx;
  ExplorerFolder* target_folder;
  int target_file_idx;
  int selected_option; // 0: New File, 1: New Folder, 2: Rename, 3: Delete
} ExplorerContextPopupState;

// Input Box State
typedef struct {
  EditorContext* ctx;
  ExplorerFolder* target_folder;
  int target_file_idx;
  ExplorerActionType action_type;
  TextBuffer input_buffer;
} ExplorerInputPopupState;

// Delete Confirmation State
typedef struct {
  EditorContext* ctx;
  ExplorerFolder* target_folder;
  int target_file_idx;
} ExplorerConfirmPopupState;

// Declarations of sub-popups
static void gui_openExplorerInputPopup(EditorContext* ctx, ExplorerFolder* folder, int file_idx,
                                       ExplorerActionType action_type);
static void gui_openExplorerConfirmPopup(EditorContext* ctx, ExplorerFolder* folder, int file_idx);

/* -------------------------------------------------------------------------- */
/*                          Helper Sync Functions                            */
/* -------------------------------------------------------------------------- */

static void syncFileDeleted(EditorContext* ctx, const char* deleted_path) {
  for (int i = 0; i < ctx->file_count; i++) {
    if (strcmp(ctx->files[i].io_file.path_abs, deleted_path) == 0) {
      if (ctx->file_count == 1) {
        ctx->files[i].io_file.status = DONT_EXIST;
      }
      else {
        int index_to_close = i;
        closeFile(&ctx->files, &ctx->file_count, &index_to_close, &ctx->refresh_local_vars);
        if (ctx->current_file_index >= ctx->file_count) {
          ctx->current_file_index = ctx->file_count - 1;
        }
        i--;
      }
    }
  }
}

static void syncFileRenamed(EditorContext* ctx, const char* old_path, const char* new_path) {
  for (int i = 0; i < ctx->file_count; i++) {
    if (strcmp(ctx->files[i].io_file.path_abs, old_path) == 0) {
      strcpy(ctx->files[i].io_file.path_abs, new_path);
      strcpy(ctx->files[i].io_file.path_args, new_path);
      ctx->files[i].feature = LF_getFeatureForFile(new_path);
      if (ctx->files[i].lsp_datas.is_enable) {
        strcpy(ctx->files[i].lsp_datas.path_abs, new_path);
      }
      ctx->gui_context.ofw_context.refresh_ofw = true;
    }
  }
}

static int remove_directory_recursive(const char* path) {
  DIR* d = opendir(path);
  size_t path_len = strlen(path);
  int r = -1;

  if (d) {
    struct dirent* p;
    r = 0;
    while (!r && (p = readdir(d))) {
      int r2 = -1;
      char* buf;
      size_t len;

      if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
        continue;
      }

      len = path_len + strlen(p->d_name) + 2;
      buf = malloc(len);

      if (buf) {
        snprintf(buf, len, "%s/%s", path, p->d_name);
        if (p->d_type == DT_DIR) {
          r2 = remove_directory_recursive(buf);
        }
        else {
          r2 = unlink(buf);
        }
        free(buf);
      }
      r = r2;
    }
    closedir(d);
  }

  if (!r) {
    r = rmdir(path);
  }
  return r;
}

static char* get_input_text(TextBuffer* tb) {
  Cursor start = tryToReachAbsPosition(tb->cursor, 1, 0);
  Cursor end = tryToReachAbsPosition(tb->cursor, INT_MAX, INT_MAX);
  return dumpSelection(start, end);
}

/* -------------------------------------------------------------------------- */
/*                         Context Menu callbacks                             */
/* -------------------------------------------------------------------------- */

static void paint_explorer_context_popup(gui_TPW* popup, void* payload) {
  ExplorerContextPopupState* state = (ExplorerContextPopupState*)payload;
  WINDOW* w = popup->tpw;
  int width = popup->width;

  werase(w);
  box(w, 0, 0);

  wattron(w, A_BOLD | COLOR_PAIR(INFO_COLOR_PAIR));
  mvwprintw(w, 0, (width - 8) / 2, " File ");
  wattroff(w, A_BOLD | COLOR_PAIR(INFO_COLOR_PAIR));

  char* options[] = {"New File", "New Folder", "Rename", "Delete"};

  for (int i = 0; i < 4; i++) {
    bool is_selected = (i == state->selected_option);
    if (is_selected) {
      wattron(w, A_REVERSE | A_BOLD | COLOR_PAIR(INFO_COLOR_PAIR));
    }
    mvwprintw(w, i + 1, 2, "%-12s", options[i]);
    if (is_selected) {
      wattroff(w, A_REVERSE | A_BOLD | COLOR_PAIR(INFO_COLOR_PAIR));
    }
  }
}

static void execute_context_action(ExplorerContextPopupState* state) {
  if (state->selected_option == 0) {
    gui_openExplorerInputPopup(state->ctx, state->target_folder, state->target_file_idx, EXPLORER_ACTION_NEW_FILE);
  }
  else if (state->selected_option == 1) {
    gui_openExplorerInputPopup(state->ctx, state->target_folder, state->target_file_idx, EXPLORER_ACTION_NEW_FOLDER);
  }
  else if (state->selected_option == 2) {
    gui_openExplorerInputPopup(state->ctx, state->target_folder, state->target_file_idx, EXPLORER_ACTION_RENAME);
  }
  else if (state->selected_option == 3) {
    gui_openExplorerConfirmPopup(state->ctx, state->target_folder, state->target_file_idx);
  }
}

static bool input_explorer_context_popup(gui_TPW* popup, int key, MEVENT* m_event, void* payload) {
  ExplorerContextPopupState* state = (ExplorerContextPopupState*)payload;
  EditorContext* ctx = state->ctx;

  // q : quit
  if (key == 'q') {
    gui_closeTPW(&ctx->gui_context, popup);
    return true;
  }

  if (key == H_KEY_UP) {
    state->selected_option = (state->selected_option - 1 + 4) % 4;
    gui_updateTPW(&ctx->gui_context);
    return true;
  }

  if (key == H_KEY_DOWN) {
    state->selected_option = (state->selected_option + 1) % 4;
    gui_updateTPW(&ctx->gui_context);
    return true;
  }

  if (key == H_KEY_ENTER || key == K_SPECIAL(0, '\r')) {
    execute_context_action(state);
    gui_closeTPW(&ctx->gui_context, popup);
    gui_updateGUI(&ctx->gui_context);
    return true;
  }

  if (key == H_KEY_MOUSE) {
    int clicked_y = m_event->y - getbegy(popup->tpw);
    int index = clicked_y - 1;
    if (index >= 0 && index < 4) {

      if (state->selected_option != index) {
        state->selected_option = index;
        gui_updateTPW(&ctx->gui_context);
      }

      if ((m_event->bstate & BUTTON1_CLICKED || m_event->bstate & BUTTON1_PRESSED)) {
        execute_context_action(state);
        gui_closeTPW(&ctx->gui_context, popup);
        gui_updateGUI(&ctx->gui_context);
      }

      return true;
    }
  }

  return false;
}

static void destroy_explorer_context_popup(gui_TPW* popup, void* payload) {
  ExplorerContextPopupState* state = (ExplorerContextPopupState*)payload;
  if (state) {
    free(state);
  }
}

void gui_openExplorerContextPopup(EditorContext* ctx, int click_y, int click_x, ExplorerFolder* folder, int file_idx) {
  ExplorerContextPopupState* state = malloc(sizeof(ExplorerContextPopupState));
  if (!state) {
    return;
  }

  state->ctx = ctx;
  state->target_folder = folder;
  state->target_file_idx = file_idx;
  state->selected_option = 0;

  int popup_w = 16;
  int popup_h = 6;

  // Bound positioning
  int y = click_y;
  int x = click_x;
  if (y + popup_h >= LINES) {
    y = LINES - popup_h - 1;
  }
  if (x + popup_w >= COLS) {
    x = COLS - popup_w - 1;
  }

  gui_createToplevelPopup(&ctx->gui_context, y, x, popup_h, popup_w, paint_explorer_context_popup,
                          input_explorer_context_popup, destroy_explorer_context_popup, state);
}

/* -------------------------------------------------------------------------- */
/*                         Input Dialog callbacks                             */
/* -------------------------------------------------------------------------- */

static void execute_input_action(ExplorerInputPopupState* state) {
  char* name = get_input_text(&state->input_buffer);
  if (strlen(name) == 0) {
    notifyUser(state->ctx, LOG_WARNING, "Name cannot be empty.");
    free(name);
    return;
  }

  char parent_path[PATH_MAX];
  strcpy(parent_path, state->target_folder->path);

  char target_path[PATH_MAX];
  snprintf(target_path, sizeof(target_path), "%s/%s", parent_path, name);

  if (state->action_type == EXPLORER_ACTION_NEW_FILE) {
    FILE* f = fopen(target_path, "w");
    if (f) {
      fclose(f);
      notifyUser(state->ctx, LOG_INFO, "File '%s' created.", name);
    }
    else {
      notifyUser(state->ctx, LOG_ERROR, "Failed to create file '%s'.", name);
    }
  }
  else if (state->action_type == EXPLORER_ACTION_NEW_FOLDER) {
    if (mkdir(target_path, 0777) == 0) {
      notifyUser(state->ctx, LOG_INFO, "Folder '%s' created.", name);
    }
    else {
      notifyUser(state->ctx, LOG_ERROR, "Failed to create folder '%s'.", name);
    }
  }
  else if (state->action_type == EXPLORER_ACTION_RENAME) {
    char old_path[PATH_MAX];
    if (state->target_file_idx == -1) {
      strcpy(old_path, state->target_folder->path);
      char old_path_copy[PATH_MAX];
      strcpy(old_path_copy, old_path);
      char* parent_dir = dirname(old_path_copy);
      snprintf(target_path, sizeof(target_path), "%s/%s", parent_dir, name);
    }
    else {
      strcpy(old_path, state->target_folder->files[state->target_file_idx].path);
    }

    if (rename(old_path, target_path) == 0) {
      notifyUser(state->ctx, LOG_INFO, "Renamed to '%s'.", name);
      if (state->target_file_idx != -1) {
        syncFileRenamed(state->ctx, old_path, target_path);
      }
    }
    else {
      notifyUser(state->ctx, LOG_ERROR, "Failed to rename.");
    }
  }

  free(name);

  // Reload tree structures
  if (state->action_type == EXPLORER_ACTION_RENAME && state->target_file_idx == -1) {
    reloadFolder(&state->ctx->pwd);
  }
  else {
    reloadFolder(state->target_folder);
  }
}

static void paint_explorer_input_popup(gui_TPW* popup, void* payload) {
  ExplorerInputPopupState* state = (ExplorerInputPopupState*)payload;
  WINDOW* w = popup->tpw;
  int width = popup->width;

  werase(w);
  box(w, 0, 0);

  wattron(w, A_BOLD | COLOR_PAIR(INFO_COLOR_PAIR));
  if (state->action_type == EXPLORER_ACTION_NEW_FILE) {
    mvwprintw(w, 0, (width - 14) / 2, " [ New File ] ");
  }
  else if (state->action_type == EXPLORER_ACTION_NEW_FOLDER) {
    mvwprintw(w, 0, (width - 16) / 2, " [ New Folder ] ");
  }
  else if (state->action_type == EXPLORER_ACTION_RENAME) {
    mvwprintw(w, 0, (width - 12) / 2, " [ Rename ] ");
  }
  wattroff(w, A_BOLD | COLOR_PAIR(INFO_COLOR_PAIR));

  mvwprintw(w, 2, 2, "Name: ");
  renderTextBuffer(w, &state->input_buffer, 8, 2, width - 10, 1);

  mvwprintw(w, 4, 2, "ENTER: Confirm | ESC: Cancel");
}

static bool input_explorer_input_popup(gui_TPW* popup, int key, MEVENT* m_event, void* payload) {
  ExplorerInputPopupState* state = (ExplorerInputPopupState*)payload;
  EditorContext* ctx = state->ctx;

  if (key == H_KEY_ENTER || key == K_SPECIAL(0, '\r')) {
    execute_input_action(state);
    gui_closeTPW(&ctx->gui_context, popup);
    gui_updateGUI(&ctx->gui_context);
    return true;
  }

  if (tb_handleKey(&state->input_buffer, key, NULL)) {
    gui_updateGUI(&ctx->gui_context);
    return true;
  }

  return true;
}

static void destroy_explorer_input_popup(gui_TPW* popup, void* payload) {
  ExplorerInputPopupState* state = (ExplorerInputPopupState*)payload;
  if (state) {
    destroyTextBuffer(&state->input_buffer);
    free(state);
  }
}

static void gui_openExplorerInputPopup(EditorContext* ctx, ExplorerFolder* folder, int file_idx,
                                       ExplorerActionType action_type) {
  ExplorerInputPopupState* state = malloc(sizeof(ExplorerInputPopupState));
  if (!state) {
    return;
  }

  state->ctx = ctx;
  state->target_folder = folder;
  state->target_file_idx = file_idx;
  state->action_type = action_type;

  initTextBuffer(&state->input_buffer, &default_feature);

  if (action_type == EXPLORER_ACTION_RENAME) {
    char* old_name = (file_idx == -1) ? basename(folder->path) : basename(folder->files[file_idx].path);
    state->input_buffer.cursor =
      insertCharArrayAtCursor(state->input_buffer.cursor, old_name, &state->input_buffer.feature->tabulation);
    setDesiredColumn(state->input_buffer.cursor, &state->input_buffer.desired_column);
  }

  gui_showTPWPositioned(&ctx->gui_context, 6, 50, GUI_TPW_POS_CENTER, paint_explorer_input_popup,
                        input_explorer_input_popup, destroy_explorer_input_popup, state);
  gui_updateGUI(&ctx->gui_context);
}

/* -------------------------------------------------------------------------- */
/*                         Confirmation callbacks                            */
/* -------------------------------------------------------------------------- */

static void execute_delete_action(ExplorerConfirmPopupState* state) {
  char path[PATH_MAX];
  bool is_dir = (state->target_file_idx == -1);

  if (is_dir) {
    strcpy(path, state->target_folder->path);
    if (remove_directory_recursive(path) == 0) {
      notifyUser(state->ctx, LOG_INFO, "Deleted folder '%s'.", basename(path));
    }
    else {
      notifyUser(state->ctx, LOG_ERROR, "Failed to delete folder.");
    }
  }
  else {
    strcpy(path, state->target_folder->files[state->target_file_idx].path);
    if (unlink(path) == 0) {
      notifyUser(state->ctx, LOG_INFO, "Deleted file '%s'.", basename(path));
      syncFileDeleted(state->ctx, path);
    }
    else {
      notifyUser(state->ctx, LOG_ERROR, "Failed to delete file.");
    }
  }

  reloadFolder(&state->ctx->pwd);
}

static void paint_explorer_confirm_popup(gui_TPW* popup, void* payload) {
  ExplorerConfirmPopupState* state = (ExplorerConfirmPopupState*)payload;
  WINDOW* w = popup->tpw;
  int width = popup->width;

  werase(w);
  box(w, 0, 0);

  char* name = (state->target_file_idx == -1) ? basename(state->target_folder->path)
                                              : basename(state->target_folder->files[state->target_file_idx].path);

  wattron(w, A_BOLD | COLOR_PAIR(WARNING_COLOR_PAIR));
  mvwprintw(w, 0, (width - 18) / 2, " [ Confirm Delete ] ");
  wattroff(w, A_BOLD | COLOR_PAIR(WARNING_COLOR_PAIR));

  mvwprintw(w, 2, 2, "Are you sure you want to delete:");
  wattron(w, A_BOLD);
  mvwprintw(w, 3, 2, "'%s'?", name);
  wattroff(w, A_BOLD);

  mvwprintw(w, 5, 2, "ENTER: Confirm | ESC: Cancel");
}

static bool input_explorer_confirm_popup(gui_TPW* popup, int key, MEVENT* m_event, void* payload) {
  ExplorerConfirmPopupState* state = (ExplorerConfirmPopupState*)payload;
  EditorContext* ctx = state->ctx;

  if (key == H_KEY_ESCAPE || key == 'n' || key == 'N' || key == K_SPECIAL(K_MOD_CTRL, '[')) {
    gui_closeTPW(&ctx->gui_context, popup);
    gui_updateGUI(&ctx->gui_context);
    return true;
  }

  if (key == H_KEY_ENTER || key == 'y' || key == 'Y' || key == K_SPECIAL(0, '\r')) {
    execute_delete_action(state);
    gui_closeTPW(&ctx->gui_context, popup);
    gui_updateGUI(&ctx->gui_context);
    return true;
  }

  return true;
}

static void destroy_explorer_confirm_popup(gui_TPW* popup, void* payload) {
  ExplorerConfirmPopupState* state = (ExplorerConfirmPopupState*)payload;
  if (state) {
    free(state);
  }
}

static void gui_openExplorerConfirmPopup(EditorContext* ctx, ExplorerFolder* folder, int file_idx) {
  ExplorerConfirmPopupState* state = malloc(sizeof(ExplorerConfirmPopupState));
  if (!state) {
    return;
  }

  state->ctx = ctx;
  state->target_folder = folder;
  state->target_file_idx = file_idx;

  gui_showTPWPositioned(&ctx->gui_context, 7, 50, GUI_TPW_POS_CENTER, paint_explorer_confirm_popup,
                        input_explorer_confirm_popup, destroy_explorer_confirm_popup, state);
  gui_updateGUI(&ctx->gui_context);
}
