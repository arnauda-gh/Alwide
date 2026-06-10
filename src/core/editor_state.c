#include "editor_state.h"
#include <stdio.h>

#include "../advanced/tree-sitter/tree_manager.h"
#include "../data-management/state_control.h"
#include "../terminal/term_handler.h"
#include "../terminal/windows/edw.h"
#include "../terminal/windows/popups/notification_popup.h"
#include "../terminal/windows/tpw.h"
#include "../utils/tools.h"
#include "editor_lsp.h"

void runBackgroundProcess(EditorContext* ctx, int* key) {
  // handle lsp servers
  handleLspServers(ctx, key);

  // Handle auto-destruction of temporary popups
  gui_checkNotificationsExpiry(&ctx->gui_context);
}

void runPostProcessing(EditorContext* ctx) {
  FileContainer* fc = &ctx->files[ctx->current_file_index];
  if (ctx->refresh_local_vars == true) {
    gui_closePopup(&ctx->gui_context);
    ctx->refresh_local_vars = false;
    ctx->old_history_frame = fc->history_frame;
    ctx->payload_state_change = getPayloadStateChange(&fc->highlight_data, &fc->lsp_datas);
    ctx->old_selected_cursor = fc->select_cursor;
    gui_updateEDW(&ctx->gui_context);
  }

  // flag cursor change
  if (!cursor_eq(fc->cursor, fc->old_cur)) {
    if (cursor_row(fc->cursor) != cursor_row(fc->old_cur)) {
      // flag cursor changed row
      if (ctx->gui_context.edw_context.pow_owner == SIGNATURE_HELP) {
        gui_closePopup(&ctx->gui_context);
      }
    }
    fc->old_cur = fc->cursor;
    moveScreenToMatchCursor(&ctx->gui_context, fc->cursor, &fc->screen_x, &fc->screen_y, LF_tab_size(fc->feature));
    gui_updateEDW(&ctx->gui_context);
  }

  // flag selection change
  if (!cursor_eq(fc->select_cursor, ctx->old_selected_cursor)) {
    ctx->old_selected_cursor = fc->select_cursor;
    gui_updateEDW(&ctx->gui_context);
    if (!cursor_is_disabled(fc->select_cursor)) {
      gui_closePopup(&ctx->gui_context);
    }
  }

  // flag screen_x change
  if (fc->old_screen_x != fc->screen_x) {
    gui_updateEDW(&ctx->gui_context);
    gui_adaptPopup(&ctx->gui_context, fc->screen_x - fc->old_screen_x, 0);
    fc->old_screen_x = fc->screen_x;
  }

  // flag screen_y change
  if (fc->old_screen_y != fc->screen_y) {
    gui_updateEDW(&ctx->gui_context);
    gui_adaptPopup(&ctx->gui_context, 0, fc->screen_y - fc->old_screen_y);

    // resize lnw_w to match with line_number_length
    int new_lnw_width = numberOfDigitOfNumber(fc->screen_y + getmaxy(ctx->gui_context.edw_context.ftw));
    if (new_lnw_width != getEDW_LengthLineNumber(&ctx->gui_context)) {
      gui_resizeEDW(&ctx->gui_context, new_lnw_width);
    }
    fc->old_screen_y = fc->screen_y;
  }

  // If it needed to reparse the current file for tree. Looking for state changes.
  if (fc->highlight_data.is_active == true &&
      (ctx->old_history_frame != fc->history_frame || fc->highlight_data.tree == NULL)) {
    parseTree(&fc->root, &fc->history_frame, &fc->highlight_data, &ctx->old_history_frame);
    optimizeHistory(fc->history_root, &fc->history_frame);
    ctx->old_history_frame = fc->history_frame;
  }
}
