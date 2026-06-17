#include "navigation_history.h"

#include <stdlib.h>
#include <string.h>

#include "../../data-management/file_management.h"
#include "../../io-management/workspace_settings.h"
#include "../../terminal/key_management.h"
#include "../../terminal/term_handler.h"
#include "../../terminal/windows/edw.h"
#include "../../utils/logger.h"
#include "../editor_context.h"

void initNavigationHistory(NavigationHistory* nh) {
  nh->back_stack.size = 0;
  nh->forward_stack.size = 0;
  nh->in_typing_session = false;
  nh->last_typing_time = 0;
}

bool getActiveNavigationLocation(struct EditorContext* ctx, NavigationLocation* loc) {
  FileContainer* fc = getActiveFile(ctx);
  if (!fc || fc->io_file.status == NONE) {
    return false;
  }
  strncpy(loc->file_path, fc->io_file.path_abs, sizeof(loc->file_path) - 1);
  loc->file_path[sizeof(loc->file_path) - 1] = '\0';
  loc->row = cursor_row(fc->cursor);
  loc->column = cursor_col(fc->cursor);
  loc->screen_x = fc->screen_x;
  loc->screen_y = fc->screen_y;
  return true;
}

void pushNavigationPoint(struct EditorContext* ctx) {
  NavigationLocation loc;
  if (!getActiveNavigationLocation(ctx, &loc)) {
    return;
  }

  // If we were in a typing session, finalize it
  if (ctx->nav_history.in_typing_session) {
    ctx->nav_history.in_typing_session = false;
  }

  NavigationStack* back = &ctx->nav_history.back_stack;

  // Avoid pushing duplicate consecutive locations in back stack
  if (back->size > 0) {
    NavigationLocation top = back->items[back->size - 1];
    if (strcmp(top.file_path, loc.file_path) == 0 && top.row == loc.row && top.column == loc.column) {
      return;
    }
  }

  if (back->size < MAX_NAV_HISTORY) {
    back->items[back->size++] = loc;
  }
  else {
    // Shift left to discard oldest
    for (int i = 1; i < MAX_NAV_HISTORY; i++) {
      back->items[i - 1] = back->items[i];
    }
    back->items[MAX_NAV_HISTORY - 1] = loc;
  }

  // Clear forward stack on new navigation/jump
  ctx->nav_history.forward_stack.size = 0;
}

static void perform_navigation_jump(struct EditorContext* ctx, NavigationLocation target) {
  // Check if file is already open
  int found_idx = -1;
  for (int i = 0; i < ctx->file_count; i++) {
    if (ctx->files[i].io_file.status != NONE && strcmp(ctx->files[i].io_file.path_abs, target.file_path) == 0) {
      found_idx = i;
      break;
    }
  }

  if (found_idx != -1) {
    ctx->current_file_index = found_idx;
  }
  else {
    // Reopen the file
    openNewFile(target.file_path, &ctx->files, &ctx->file_count, &ctx->current_file_index,
                &ctx->gui_context.ofw_context.refresh_ofw, &ctx->refresh_local_vars);
  }

  FileContainer* fc = getActiveFile(ctx);
  if (!fc) {
    return;
  }

  // Restore cursor and view position
  fc->cursor = tryToReachAbsPosition(fc->cursor, target.row, target.column);
  fc->desired_column = target.column;
  fc->screen_x = target.screen_x;
  fc->screen_y = target.screen_y;

  ctx->refresh_local_vars = true;
  gui_updateOFW(&ctx->gui_context);
  gui_updateEDW(&ctx->gui_context);
  gui_closeEDWPopup(&ctx->gui_context);
}

void navigateBack(struct EditorContext* ctx) {
  // If we are in a typing session, finalize it before navigating back
  if (ctx->nav_history.in_typing_session) {
    ctx->nav_history.in_typing_session = false;
  }

  NavigationStack* back = &ctx->nav_history.back_stack;
  if (back->size == 0) {
    return;
  }

  NavigationLocation current_loc;
  if (!getActiveNavigationLocation(ctx, &current_loc)) {
    return;
  }

  // Pop target location from back stack
  NavigationLocation target_loc = back->items[--back->size];

  // Push current location to forward stack
  NavigationStack* forward = &ctx->nav_history.forward_stack;
  if (forward->size < MAX_NAV_HISTORY) {
    forward->items[forward->size++] = current_loc;
  }
  else {
    for (int i = 1; i < MAX_NAV_HISTORY; i++) {
      forward->items[i - 1] = forward->items[i];
    }
    forward->items[MAX_NAV_HISTORY - 1] = current_loc;
  }

  perform_navigation_jump(ctx, target_loc);
}

void navigateForward(struct EditorContext* ctx) {
  // If we are in a typing session, finalize it before navigating forward
  if (ctx->nav_history.in_typing_session) {
    ctx->nav_history.in_typing_session = false;
  }

  NavigationStack* forward = &ctx->nav_history.forward_stack;
  if (forward->size == 0) {
    return;
  }

  NavigationLocation current_loc;
  if (!getActiveNavigationLocation(ctx, &current_loc)) {
    return;
  }

  // Pop target location from forward stack
  NavigationLocation target_loc = forward->items[--forward->size];

  // Push current location to back stack
  NavigationStack* back = &ctx->nav_history.back_stack;
  if (back->size < MAX_NAV_HISTORY) {
    back->items[back->size++] = current_loc;
  }
  else {
    for (int i = 1; i < MAX_NAV_HISTORY; i++) {
      back->items[i - 1] = back->items[i];
    }
    back->items[MAX_NAV_HISTORY - 1] = current_loc;
  }

  perform_navigation_jump(ctx, target_loc);
}

static bool is_edit_key(int key) {
  if (!K_IS_SPECIAL(key)) {
    return true; // Any printable character
  }
  int code = K_CODE(key);
  int mods = key & (K_MOD_SHIFT | K_MOD_ALT | K_MOD_CTRL | K_MOD_SUPER);

  if (key == H_KEY_ENTER || key == H_KEY_SHIFT_ENTER || key == H_KEY_BACKSPACE || key == H_KEY_CTRL_DELETE ||
      key == H_KEY_SUPPR || key == H_KEY_CTRL_SUPPR || key == H_KEY_TAB || key == H_KEY_SHIFT_TAB) {
    return true;
  }

  // Ctrl+V (paste), Ctrl+X (cut), Ctrl+Z (undo), Ctrl+Y (redo), Ctrl+_ (comment)
  if (mods == K_MOD_CTRL) {
    if (code == 'v' || code == 'x' || code == 'z' || code == 'y' || code == '_') {
      return true;
    }
  }
  // Ctrl+Shift+: (comment)
  if (mods == (K_MOD_CTRL | K_MOD_SHIFT) && code == ':') {
    return true;
  }

  return false;
}

static bool is_significant_distance(NavigationLocation loc1, NavigationLocation loc2) {
  if (strcmp(loc1.file_path, loc2.file_path) != 0) {
    return true;
  }
  if (abs(loc1.row - loc2.row) > 1) {
    return true;
  }
  if (abs(loc1.column - loc2.column) > 10) {
    return true;
  }
  return false;
}

void handleNavigationHistoryEvent(struct EditorContext* ctx, const NavigationLocation* prev_loc, int key) {
  if (!prev_loc) {
    return;
  }

  if (key == K_SPECIAL(K_MOD_CTRL, 'u') || key == K_SPECIAL(K_MOD_CTRL, 'p')) {
    return;
  }

  NavigationLocation curr_loc;
  if (!getActiveNavigationLocation(ctx, &curr_loc)) {
    return;
  }

  // 1. Check if we switched files
  if (strcmp(prev_loc->file_path, curr_loc.file_path) != 0) {
    if (ctx->nav_history.in_typing_session) {
      ctx->nav_history.in_typing_session = false;
    }

    NavigationStack* back = &ctx->nav_history.back_stack;
    if (back->size == 0 || strcmp(back->items[back->size - 1].file_path, prev_loc->file_path) != 0 ||
        back->items[back->size - 1].row != prev_loc->row || back->items[back->size - 1].column != prev_loc->column) {

      if (back->size < MAX_NAV_HISTORY) {
        back->items[back->size++] = *prev_loc;
      }
      else {
        for (int i = 1; i < MAX_NAV_HISTORY; i++) {
          back->items[i - 1] = back->items[i];
        }
        back->items[MAX_NAV_HISTORY - 1] = *prev_loc;
      }
      notifyUser(ctx, LOG_INFO, "handle_navigation_history_event: pushed file switch from %s to %s, back size=%d",
                 prev_loc->file_path, curr_loc.file_path, back->size);
      ctx->nav_history.forward_stack.size = 0;
    }
    return;
  }

  // 2. Same file. Check if an edit key was pressed.
  bool is_edit = is_edit_key(key);

  if (is_edit) {
    time_val now = timeInMilliseconds();

    if (!ctx->nav_history.in_typing_session) {
      // Start typing session
      ctx->nav_history.in_typing_session = true;
      ctx->nav_history.last_typing_time = now;
      ctx->nav_history.typing_start_location = *prev_loc;

      // Push typing start point to back stack
      NavigationLocation loc = ctx->nav_history.typing_start_location;
      NavigationStack* back = &ctx->nav_history.back_stack;
      if (back->size == 0 || strcmp(back->items[back->size - 1].file_path, loc.file_path) != 0 ||
          back->items[back->size - 1].row != loc.row || back->items[back->size - 1].column != loc.column) {

        if (back->size < MAX_NAV_HISTORY) {
          back->items[back->size++] = loc;
        }
        else {
          for (int i = 1; i < MAX_NAV_HISTORY; i++) {
            back->items[i - 1] = back->items[i];
          }
          back->items[MAX_NAV_HISTORY - 1] = loc;
        }
        ctx->nav_history.forward_stack.size = 0;
      }
    }
    else {
      // Continue typing session. Check time gap
      if (diff2Time(now, ctx->nav_history.last_typing_time) > 3000) {
        // Finalize old session and push end location if significant
        if (is_significant_distance(ctx->nav_history.typing_start_location, *prev_loc)) {
          NavigationStack* back = &ctx->nav_history.back_stack;
          if (back->size < MAX_NAV_HISTORY) {
            back->items[back->size++] = *prev_loc;
          }
          else {
            for (int i = 1; i < MAX_NAV_HISTORY; i++) {
              back->items[i - 1] = back->items[i];
            }
            back->items[MAX_NAV_HISTORY - 1] = *prev_loc;
          }
        }

        // Start new session
        ctx->nav_history.typing_start_location = *prev_loc;

        NavigationLocation loc = ctx->nav_history.typing_start_location;
        NavigationStack* back = &ctx->nav_history.back_stack;
        if (back->size < MAX_NAV_HISTORY) {
          back->items[back->size++] = loc;
        }
        else {
          for (int i = 1; i < MAX_NAV_HISTORY; i++) {
            back->items[i - 1] = back->items[i];
          }
          back->items[MAX_NAV_HISTORY - 1] = loc;
        }
      }
      ctx->nav_history.last_typing_time = now;
    }
  }
  else {
    // Non-edit action. If we were typing, check if the cursor moved to finalize
    if (ctx->nav_history.in_typing_session) {
      if (prev_loc->row != curr_loc.row || prev_loc->column != curr_loc.column) {
        ctx->nav_history.in_typing_session = false;

        if (is_significant_distance(ctx->nav_history.typing_start_location, *prev_loc)) {
          NavigationStack* back = &ctx->nav_history.back_stack;
          if (back->size < MAX_NAV_HISTORY) {
            back->items[back->size++] = *prev_loc;
          }
          else {
            for (int i = 1; i < MAX_NAV_HISTORY; i++) {
              back->items[i - 1] = back->items[i];
            }
            back->items[MAX_NAV_HISTORY - 1] = *prev_loc;
          }
        }
      }
    }
  }
}

void restoreNavigationHistory(NavigationHistory* nh, struct WorkspaceSettings* settings) {
  nh->back_stack.size = settings->nav_back_size;
  for (int i = 0; i < settings->nav_back_size; i++) {
    nh->back_stack.items[i] = settings->nav_back_items[i];
  }

  nh->forward_stack.size = settings->nav_forward_size;
  for (int i = 0; i < settings->nav_forward_size; i++) {
    nh->forward_stack.items[i] = settings->nav_forward_items[i];
  }
}

void saveNavigationHistoryToSettings(struct WorkspaceSettings* settings, NavigationHistory* nh) {
  settings->nav_back_size = nh->back_stack.size;
  if (nh->back_stack.size > 0) {
    settings->nav_back_items = malloc(nh->back_stack.size * sizeof(NavigationLocation));
    for (int i = 0; i < nh->back_stack.size; i++) {
      settings->nav_back_items[i] = nh->back_stack.items[i];
    }
  }
  else {
    settings->nav_back_items = NULL;
  }

  settings->nav_forward_size = nh->forward_stack.size;
  if (nh->forward_stack.size > 0) {
    settings->nav_forward_items = malloc(nh->forward_stack.size * sizeof(NavigationLocation));
    for (int i = 0; i < nh->forward_stack.size; i++) {
      settings->nav_forward_items[i] = nh->forward_stack.items[i];
    }
  }
  else {
    settings->nav_forward_items = NULL;
  }
}
