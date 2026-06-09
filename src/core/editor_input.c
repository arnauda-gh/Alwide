#include "editor_input.h"

#include <assert.h>
#include <limits.h>
#include <ncurses.h>
#include <stdio.h>
#include <time.h>

#include "../advanced/intelligence/auto_pairs.h"
#include "../advanced/intelligence/comments.h"
#include "../advanced/intelligence/indentation.h"
#include "../advanced/lsp/lsp-features/lsp_completion.h"
#include "../advanced/lsp/lsp-features/lsp_formatting.h"
#include "../advanced/lsp/lsp-features/lsp_signature_help.h"
#include "../advanced/lsp/lsp_dispatcher.h"
#include "../data-management/state_control.h"
#include "../environnement/global_variables.h"
#include "../io-management/io_manager.h"
#include "../terminal/click_handler.h"
#include "../terminal/key_management.h"
#include "../terminal/kitty_protocol.h"
#include "../terminal/windows/few.h"
#include "../terminal/windows/ofw.h"
#include "../terminal/windows/popups/search_popup.h"
#include "../terminal/windows/pow.h"
#include "../utils/clipboard_manager.h"
#include "../utils/tools.h"
#include "editor_lsp.h"

EventLoopAction dispatchInput(EditorContext* ctx, int key) {
  // if input is empty, execute nothing and read again
  if (key == ERR) {
    return EVENT_READ_INPUT;
  }

  EventLoopAction loopEnd = EVENT_READ_INPUT;
  if (runInternalLogic(ctx, key, &loopEnd)) {
    return loopEnd;
  }

  if (handlePopupInput(ctx, key)) {
    return EVENT_READ_INPUT;
  }

  /* Route all functional navigation, hotkeys, and releases to the command handler */
  if (K_IS_SPECIAL(key)) {
    return runSpecialKeyHandler(ctx, key);
  }

  /*
   * Unified Routing:
   * Any key NOT marked as Special is a printable character.
   */
  handleCharInsertion(ctx, key);
  return EVENT_CONTINUE;
}


bool handlePopupInput(EditorContext* ctx, int key) {
  // Route keyboard input to the focused active toplevel popup first
  if (dispatchInputToTPW(ctx, key)) {
    return true;
  }

  // Next route input to the edw internal popup system
  FileContainer* fc = &ctx->files[ctx->current_file_index];
  ModuleContext payload = buildModuleContext(ctx);
  bool result = gui_handlePopupInput(&ctx->gui_context, fc, key, &ctx->payload_state_change, &payload, &ctx->m_event);

  return result;
}

bool runInternalLogic(EditorContext* ctx, int key, EventLoopAction* out_action) {
  if (key == K_SPECIAL(K_MOD_CTRL, 'q')) {
    *out_action = EVENT_QUIT;
    return true;
  }

  switch (key) {
    case ONLY_REPAINT_INPUT:
      gui_updateGUI(&ctx->gui_context);
      *out_action = EVENT_CONTINUE;
      return true;

    case BEGIN_MOUSE_LISTEN:
    case MOUSE_IN_OUT:
    case H_KEY_RESIZE:
      gui_resizeOFW(&ctx->gui_context);
      gui_resizeEDW(&ctx->gui_context, -1);
      gui_updateGUI(&ctx->gui_context);
      *out_action = EVENT_CONTINUE;
      return true;

    default:
      return false;
  }
}

/**
 * --- Static Helpers for Input Processing ---
 */

static int getRawByte(EditorContext* ctx) {
  if (ctx->peek_c == -1) {
    return getch();
  }
  int c = ctx->peek_c;
  ctx->peek_c = -1;
  return c;
}

static int parseEscapeSequence(EditorContext* ctx, int first_char) {
  KittyKeyEvent kitty_event;
  int unread = ERR;
  int key = ERR;

  if (kitty_parse_sequence(first_char, &kitty_event, &ctx->m_event, &unread)) {
    if (unread != ERR) {
      ctx->peek_c = unread;
    }
    if (!kitty_translate_event(&kitty_event, &key)) {
      return ERR;
    }
  }
  return key;
}

static int manualUtf8Decode(int first_byte) {
  unsigned char first = (unsigned char)first_byte;
  int codepoint = 0;
  int bytes_to_read = 0;

  if ((first & 0xE0) == 0xC0) {
    bytes_to_read = 1;
    codepoint = first & 0x1F;
  }
  else if ((first & 0xF0) == 0xE0) {
    bytes_to_read = 2;
    codepoint = first & 0x0F;
  }
  else if ((first & 0xF8) == 0xF0) {
    bytes_to_read = 3;
    codepoint = first & 0x07;
  }

  if (bytes_to_read > 0) {
    bool valid = true;
    for (int i = 0; i < bytes_to_read; i++) {
      int next_byte = getch();
      if (next_byte == ERR || (next_byte & 0xC0) != 0x80) {
        valid = false;
        break;
      }
      codepoint = (codepoint << 6) | (next_byte & 0x3F);
    }
    if (valid) {
      return codepoint;
    }
  }
  return ERR;
}

static void processMouseInput(EditorContext* ctx) {
  detectComplexMouseEvents(&ctx->m_event);

  // Coalesce mouse drag/movement events to prevent input queue lag
  if (ctx->m_event.bstate == NO_EVENT_MOUSE && ctx->mouse_drag == true) {
    while (true) {
      timeout(0); // Non-blocking peek
      int next_c = getch();
      timeout(TIME_OUT_DELAY); // Restore normal timeout

      if (next_c == ERR) {
        break;
      }

      if (next_c == 27) {
        KittyKeyEvent kitty_event;
        int unread = ERR;
        MEVENT tmp_event;
        if (kitty_parse_sequence(next_c, &kitty_event, &tmp_event, &unread)) {
          if (unread != ERR) {
            ctx->peek_c = unread;
          }
          int key = ERR;
          if (kitty_translate_event(&kitty_event, &key)) {
            if (key == H_KEY_MOUSE) {
              // Overwrite current m_event with the newly parsed mouse event
              ctx->m_event = tmp_event;
              detectComplexMouseEvents(&ctx->m_event);
              ctx->t_date = timeInMilliseconds();
              ctx->t_clock = clock();

              if (tmp_event.bstate == NO_EVENT_MOUSE) {
                fprintf(stderr, "Mouse event skipped !\n");
                // It is a drag/move event: we can continue coalescing
                continue;
              }
              else {
                // It is a press or release event: stop coalescing immediately so it gets processed
                break;
              }
            }
            else if (key != ERR) {
              ctx->peek_c = key;
              break;
            }
          }
        }
      }
      else {
        ctx->peek_c = next_c;
        break;
      }
    }
  }
}

int readNextInput(EditorContext* ctx) {
  int c = getRawByte(ctx);
  if (c == ERR) {
    return ERR;
  }

  ctx->t_date = timeInMilliseconds();
  ctx->t_clock = clock();

  int key = ERR;

  /* Tier 1: Kitty/ANSI Escape sequences */
  if (c == 27) {
    key = parseEscapeSequence(ctx, c);
  }

  /* Tier 2: Legacy Fallback (UTF-8 or Normalization) */
  if (key == ERR) {
    if (c == KEY_MOUSE) {
      key = H_KEY_MOUSE;
    }
    else if ((c & 0x80) != 0 && c <= 255) {
      key = manualUtf8Decode(c);
      if (key == ERR) {
        key = normalize_legacy(c);
      }
    }
    else {
      key = normalize_legacy(c);
    }
  }

  /* Mouse Event Handling */
  if (key == H_KEY_MOUSE) {
    processMouseInput(ctx);
  }

  return key;
}

void handleCharInsertion(EditorContext* ctx, int key) {
  FileContainer* fc = &ctx->files[ctx->current_file_index];

  /* Extract printable codepoint (Bits 0-23) */
  int codepoint = K_CODE(key);
  assert(codepoint == key);

  /* Ignore control characters, except Tab and Newline (though those are usually Special keys) */
  if (codepoint == ERR || (codepoint < 32 && codepoint != '\t' && codepoint != '\n') || codepoint == 127) {
    return;
  }

  Cursor* cursor = &fc->cursor;
  Cursor* select_cursor = &fc->select_cursor;
  int* desired_column = &fc->desired_column;
  History** history_frame = &fc->history_frame;

  deleteSelectionWithState(history_frame, cursor, select_cursor, &ctx->payload_state_change);
  CursorDescriptor tmp = cursor_to_desc(*cursor);
  Char_U8 u8 = unicode_to_utf8(codepoint);

  if (!ilj_handleAutoPairs(fc, u8, history_frame, ctx->payload_state_change)) {
    *cursor = insertCharInLineC(*cursor, u8);
    saveAction(history_frame, createInsertAction(*cursor, tmp), globalOnStateChange, cursor,
               (long*)&ctx->payload_state_change);
  }

  setDesiredColumn(*cursor, desired_column);

  if (ctx->gui_context.edw_context.pow_owner == DIAGNOSTICS) {
    gui_closePopup(&ctx->gui_context);
  }

  ModuleContext lsp_ctx = buildModuleContext(ctx);
  askOnTypeFormatting(fc, u8.t, &lsp_ctx);
  askOnCharTypeLspInfos(ctx, key, fc, cursor);
}

static void handleTabInsertion(FileContainer* fc, Cursor* cursor) {
  if (!LF_tab_use_space(fc->feature)) {
    *cursor = insertCharInLineC(*cursor, unicode_to_utf8('\t'));
  }
  else {
    int tab_size = LF_tab_size(fc->feature);
    for (int i = 0; i < tab_size; i++) {
      *cursor = insertCharInLineC(*cursor, unicode_to_utf8(' '));
    }
  }
}

EventLoopAction runSpecialKeyHandler(EditorContext* ctx, int key) {
  FileContainer* fc = &ctx->files[ctx->current_file_index];

  Cursor* cursor = &fc->cursor;
  Cursor* select_cursor = &fc->select_cursor;
  int* desired_column = &fc->desired_column;
  int* screen_x = &fc->screen_x;
  int* screen_y = &fc->screen_y;
  History** history_frame = &fc->history_frame;
  History** history_root = &fc->history_root;
  IO_FileID* io_file = &fc->io_file;
  FileNode** root = &fc->root;
  LSP_Data* lsp_data = &fc->lsp_datas;
  CursorDescriptor tmp;

  ModuleContext lsp_ctx = buildModuleContext(ctx);

  switch (key) {
    case H_KEY_MOUSE:
      handleClick(ctx);
      break;
    case K_SPECIAL(K_MOD_CTRL, 'k'):
    case H_KEY_RIGHT:
      if (cursor_is_disabled(*select_cursor)) {
        *cursor = moveRight(*cursor);
        if (areChar_U8Equals(getCharAtCursor(*cursor), readChar_U8FromCharArray(")")) &&
            ctx->gui_context.edw_context.pow_owner == SIGNATURE_HELP) {
          gui_closePopup(&ctx->gui_context);
        }
      }
      setSelectCursorOff(cursor, select_cursor, SELECT_OFF_RIGHT);
      setDesiredColumn(*cursor, desired_column);
      askCompletion(&ctx->gui_context, fc, false, false);
      break;
    case K_SPECIAL(K_MOD_CTRL, 'j'):
    case H_KEY_LEFT:
      if (cursor_is_disabled(*select_cursor)) {
        if (areChar_U8Equals(getCharAtCursor(*cursor), readChar_U8FromCharArray("(")) &&
            ctx->gui_context.edw_context.pow_owner == SIGNATURE_HELP) {
          gui_closePopup(&ctx->gui_context);
        }
        *cursor = moveLeft(*cursor);
      }
      setSelectCursorOff(cursor, select_cursor, SELECT_OFF_LEFT);
      setDesiredColumn(*cursor, desired_column);
      askCompletion(&ctx->gui_context, fc, false, false);
      break;
    case K_SPECIAL(K_MOD_CTRL, 'i'):
    case H_KEY_UP:
      *cursor = moveUp(*cursor, *desired_column);
      setSelectCursorOff(cursor, select_cursor, SELECT_OFF_LEFT);
      gui_closePopup(&ctx->gui_context);
      break;
    case K_SPECIAL(K_MOD_CTRL, 'n'):
    case H_KEY_DOWN:
      *cursor = moveDown(*cursor, *desired_column);
      setSelectCursorOff(cursor, select_cursor, SELECT_OFF_RIGHT);
      gui_closePopup(&ctx->gui_context);
      break;
    case K_SPECIAL(K_MOD_CTRL | K_MOD_SHIFT, 'k'):
    case H_KEY_MAJ_RIGHT:
      setSelectCursorOn(*cursor, select_cursor);
      *cursor = moveRight(*cursor);
      setDesiredColumn(*cursor, desired_column);
      break;
    case K_SPECIAL(K_MOD_CTRL | K_MOD_SHIFT, 'j'):
    case H_KEY_MAJ_LEFT:
      setSelectCursorOn(*cursor, select_cursor);
      *cursor = moveLeft(*cursor);
      setDesiredColumn(*cursor, desired_column);
      break;
    case K_SPECIAL(K_MOD_CTRL | K_MOD_SHIFT, 'i'):
    case H_KEY_MAJ_UP:
      setSelectCursorOn(*cursor, select_cursor);
      *cursor = moveUp(*cursor, *desired_column);
      gui_closePopup(&ctx->gui_context);
      break;
    case K_SPECIAL(K_MOD_CTRL | K_MOD_SHIFT, 'n'):
    case H_KEY_MAJ_DOWN:
      setSelectCursorOn(*cursor, select_cursor);
      *cursor = moveDown(*cursor, *desired_column);
      gui_closePopup(&ctx->gui_context);
      break;
    case H_KEY_CTRL_RIGHT:
      setSelectCursorOff(cursor, select_cursor, SELECT_OFF_RIGHT);
      *cursor = moveToNextWord(*cursor);
      if (areChar_U8Equals(getCharAtCursor(*cursor), readChar_U8FromCharArray(")")) &&
          ctx->gui_context.edw_context.pow_owner == SIGNATURE_HELP) {
        gui_closePopup(&ctx->gui_context);
      }
      setDesiredColumn(*cursor, desired_column);
      askCompletion(&ctx->gui_context, fc, true, false);
      break;
    case H_KEY_CTRL_LEFT:
      setSelectCursorOff(cursor, select_cursor, SELECT_OFF_LEFT);
      if (areChar_U8Equals(getCharAtCursor(*cursor), readChar_U8FromCharArray("(")) &&
          ctx->gui_context.edw_context.pow_owner == SIGNATURE_HELP) {
        gui_closePopup(&ctx->gui_context);
      }
      *cursor = moveToPreviousWord(*cursor);
      setDesiredColumn(*cursor, desired_column);
      askCompletion(&ctx->gui_context, fc, true, false);
      break;
    case H_KEY_CTRL_DOWN:
    case H_KEY_CTRL_UP:
      selectWord(cursor, select_cursor);
      break;
    case H_KEY_CTRL_MAJ_RIGHT:
      setSelectCursorOn(*cursor, select_cursor);
      *cursor = moveToNextWord(*cursor);
      setDesiredColumn(*cursor, desired_column);
      break;
    case H_KEY_CTRL_MAJ_LEFT:
      setSelectCursorOn(*cursor, select_cursor);
      *cursor = moveToPreviousWord(*cursor);
      setDesiredColumn(*cursor, desired_column);
      break;
    case H_KEY_CTRL_MAJ_DOWN:
      if (ctx->current_file_index != 0) {
        ctx->current_file_index--;
      }
      ctx->refresh_local_vars = true;
      gui_updateOFW(&ctx->gui_context);
      gui_closePopup(&ctx->gui_context);
      break;
    case H_KEY_CTRL_MAJ_UP:
      if (ctx->current_file_index != ctx->file_count - 1) {
        ctx->current_file_index++;
      }
      ctx->refresh_local_vars = true;
      gui_updateOFW(&ctx->gui_context);
      gui_closePopup(&ctx->gui_context);
      break;
    case K_SPECIAL(K_MOD_CTRL, 'r'):
      askFormatting(fc);
      break;

    case H_KEY_BEGIN:
      setSelectCursorOff(cursor, select_cursor, SELECT_OFF_LEFT);
      *cursor = goToBegin(*cursor);
      setDesiredColumn(*cursor, desired_column);
      setSelectCursorOff(cursor, select_cursor, SELECT_OFF_RIGHT);
      *cursor = moveToNextWord(*cursor);
      setDesiredColumn(*cursor, desired_column);
      setSelectCursorOff(cursor, select_cursor, SELECT_OFF_LEFT);
      *cursor = moveToPreviousWord(*cursor);
      setDesiredColumn(*cursor, desired_column);
      gui_closePopup(&ctx->gui_context);
      break;
    case H_KEY_CTRL_SEMICOLON:
    case H_KEY_END:
      setSelectCursorOff(cursor, select_cursor, SELECT_OFF_RIGHT);
      *cursor = goToEnd(*cursor);
      setDesiredColumn(*cursor, desired_column);
      break;
    case H_KEY_MAJ_END:
      setSelectCursorOn(*cursor, select_cursor);
      *cursor = goToEnd(*cursor);
      setDesiredColumn(*cursor, desired_column);
      gui_closePopup(&ctx->gui_context);
      break;
    case K_SPECIAL(K_MOD_CTRL, 'z'):
      setSelectCursorOff(cursor, select_cursor, SELECT_OFF_LEFT);
      *cursor =
        undo(history_frame, *cursor, globalOnStateChange, (long*)&ctx->payload_state_change, LF_tab(fc->feature));
      ctx->old_history_frame = NULL;
      setDesiredColumn(*cursor, desired_column);
      gui_updateEDW(&ctx->gui_context);
      askCompletion(&ctx->gui_context, fc, true, false);
      break;
    case K_SPECIAL(K_MOD_CTRL, 'y'):
      setSelectCursorOff(cursor, select_cursor, SELECT_OFF_LEFT);
      *cursor =
        redo(history_frame, *cursor, globalOnStateChange, (long*)&ctx->payload_state_change, LF_tab(fc->feature));
      ctx->old_history_frame = NULL;
      setDesiredColumn(*cursor, desired_column);
      gui_updateEDW(&ctx->gui_context);
      askCompletion(&ctx->gui_context, fc, true, false);
      break;
    case K_SPECIAL(K_MOD_CTRL, 'c'):
      saveToClipBoard(*cursor, *select_cursor);
      break;
    case K_SPECIAL(K_MOD_CTRL, 'a'):
      *select_cursor = tryToReachAbsPosition(*cursor, 1, 0);
      *cursor = tryToReachAbsPosition(*cursor, INT_MAX, INT_MAX);
      break;
    case K_SPECIAL(K_MOD_CTRL, 'f'):
      {
        char* query = dumpSelection(*cursor, *select_cursor);
        gui_openSearchPopup(ctx, query);
        free(query);
        break;
      }
    case K_SPECIAL(K_MOD_CTRL, 'x'):
      saveToClipBoard(*cursor, *select_cursor);
      deleteSelectionWithState(history_frame, cursor, select_cursor, &ctx->payload_state_change);
      setDesiredColumn(*cursor, desired_column);
      gui_updateEDW(&ctx->gui_context);
      break;
    case K_SPECIAL(K_MOD_CTRL, 'v'):
      deleteSelectionWithState(history_frame, cursor, select_cursor, &ctx->payload_state_change);
      tmp = cursor_to_desc(*cursor);
      *cursor = loadFromClipBoard(fc);
      saveAction(history_frame, createInsertAction(*cursor, tmp), globalOnStateChange, cursor,
                 (long*)&ctx->payload_state_change);
      setDesiredColumn(*cursor, desired_column);
      break;
    case K_SPECIAL(K_MOD_CTRL | K_MOD_SHIFT, ':'):
    case K_SPECIAL(K_MOD_CTRL, '_'):
      ilj_toggleComments(fc, history_frame, &ctx->payload_state_change);
      gui_updateEDW(&ctx->gui_context);
      break;
    case K_SPECIAL(K_MOD_CTRL, 'q'):
      return EVENT_QUIT;
    case K_SPECIAL(K_MOD_CTRL, 'w'):
      closeFile(&ctx->files, &ctx->file_count, &ctx->current_file_index, &ctx->refresh_local_vars);
      gui_updateOFW(&ctx->gui_context);
      gui_updateEDW(&ctx->gui_context);
      break;
    case K_SPECIAL(K_MOD_CTRL, 's'):
      if (io_file->status == NONE) {
        printf("\r\nNo specified file\r\n");
        return EVENT_QUIT;
      }
      if (lsp_data->is_enable) {
        askFormatting(fc);
        waitForLspResponse(ctx, 200);
      }
      saveFile(*root, io_file);
      assert(io_file->status == EXIST);
      setlastFilePosition(io_file->path_abs, cursor_row(*cursor), cursor_col(*cursor), *screen_x, *screen_y);
      saveCurrentStateControl(**history_root, *history_frame, io_file->path_abs);
      break;
    case H_KEY_CTRL_DELETE:
    case K_SPECIAL(K_MOD_CTRL, 'h'):
      {
        setSelectCursorOn(*cursor, select_cursor);
        *cursor = moveToPreviousWord(*cursor);
        bool need_reask_signature = adaptSignatureHelpOnDelete(*cursor, *select_cursor, lsp_data, ctx);
        deleteSelectionWithState(history_frame, cursor, select_cursor, &ctx->payload_state_change);
        setDesiredColumn(*cursor, desired_column);
        if (need_reask_signature) {
          askSignatureHelp(getActiveFile(ctx), cursor);
        }
        askCompletion(&ctx->gui_context, fc, false, false);
      }
      break;
    case H_KEY_SHIFT_ENTER:
      *cursor = goToEnd(*cursor);
    case H_KEY_ENTER:
      gui_closePopup(&ctx->gui_context);
      deleteSelectionWithState(history_frame, cursor, select_cursor, &ctx->payload_state_change);
      tmp = cursor_to_desc(*cursor);
      *cursor = insertNewLineInLineC(*cursor);
      saveAction(history_frame, createInsertAction(*cursor, tmp), globalOnStateChange, cursor,
                 (long*)&ctx->payload_state_change);
      setDesiredColumn(*cursor, desired_column);
      askOnTypeFormatting(fc, "\n", &lsp_ctx);
      break;
    case H_KEY_DELETE:
      {
        if (cursor_is_disabled(*select_cursor)) {
          if (ilj_handleAutoPairDelete(fc, history_frame, ctx->payload_state_change)) {
            setDesiredColumn(*cursor, desired_column);
            askCompletion(&ctx->gui_context, fc, false, false);
            break;
          }
          *select_cursor = moveLeft(*cursor);
        }
        bool need_reask_signature = adaptSignatureHelpOnDelete(*cursor, *select_cursor, lsp_data, ctx);
        deleteSelectionWithState(history_frame, cursor, select_cursor, &ctx->payload_state_change);
        setDesiredColumn(*cursor, desired_column);
        if (need_reask_signature) {
          askSignatureHelp(getActiveFile(ctx), cursor);
        }
        askCompletion(&ctx->gui_context, fc, false, false);
      }
      break;
    case H_KEY_SUPPR:
      if (cursor_is_disabled(*select_cursor)) {
        *select_cursor = moveRight(*cursor);
      }
      deleteSelectionWithState(history_frame, cursor, select_cursor, &ctx->payload_state_change);
      setDesiredColumn(*cursor, desired_column);
      askCompletion(&ctx->gui_context, fc, false, false);
      gui_updateEDW(&ctx->gui_context);
      break;
    case H_KEY_CTRL_SUPPR:
      setSelectCursorOn(*cursor, select_cursor);
      *cursor = moveToNextWord(*cursor);
      deleteSelectionWithState(history_frame, cursor, select_cursor, &ctx->payload_state_change);
      setDesiredColumn(*cursor, desired_column);
      askCompletion(&ctx->gui_context, fc, false, false);
      gui_updateEDW(&ctx->gui_context);
      break;
    case H_KEY_TAB:
      if (!cursor_is_disabled(*select_cursor) && cursor->file_id.absolute_row != select_cursor->file_id.absolute_row) {
        ilf_indentSelectedLines(fc, history_frame, &ctx->payload_state_change);
        gui_updateEDW(&ctx->gui_context);
      }
      else {
        deleteSelectionWithState(history_frame, cursor, select_cursor, &ctx->payload_state_change);
        tmp = cursor_to_desc(*cursor);
        handleTabInsertion(fc, cursor);
        saveAction(history_frame, createInsertAction(*cursor, tmp), globalOnStateChange, cursor,
                   (long*)&ctx->payload_state_change);
        setDesiredColumn(*cursor, desired_column);
        askOnTypeFormatting(fc, "\t", &lsp_ctx);
      }
      break;
    case H_KEY_SHIFT_TAB:
      ilf_deindentSelectedLines(fc, history_frame, &ctx->payload_state_change);
      gui_updateEDW(&ctx->gui_context);
      break;
    case K_SPECIAL(K_MOD_CTRL, 'd'):
      if (cursor_is_disabled(*select_cursor) == true) {
        selectLine(cursor, select_cursor);
      }
      deleteSelectionWithState(history_frame, cursor, select_cursor, &ctx->payload_state_change);
      setDesiredColumn(*cursor, desired_column);
      gui_closePopup(&ctx->gui_context);
      gui_updateEDW(&ctx->gui_context);
      break;
    case K_SPECIAL(K_MOD_CTRL, 'e'):
      gui_switchFEW(&ctx->gui_context);
      break;
    case K_SPECIAL(K_MOD_CTRL, 'b'):
      gui_switchSBW(&ctx->gui_context);
      gui_updateGUI(&ctx->gui_context);
      break;
    case K_SPECIAL(K_MOD_CTRL, 'l'):
      gui_switchOFW(&ctx->gui_context);
      break;
    case K_SPECIAL(K_MOD_CTRL, ' '):
      askCompletion(&ctx->gui_context, fc, true, true);
      break;
    case H_KEY_ESCAPE:
    case K_SPECIAL(K_MOD_CTRL, '['):
      gui_closePopup(&ctx->gui_context);
      break;
    default:
      /* Unmapped command, release, or hotkey sequence: safely ignore it */
      logInput(key);
      break;
  }
  return EVENT_CONTINUE;
}
