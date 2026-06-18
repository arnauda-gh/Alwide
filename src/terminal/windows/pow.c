#include "pow.h"

#include <assert.h>
#include <libgen.h>
#include <string.h>

#include "../../advanced/lsp/lsp-features/lsp_code_action.h"
#include "../../advanced/lsp/lsp-features/lsp_completion.h"
#include "../../advanced/lsp/lsp-features/lsp_goto.h"
#include "../../io-management/viewport_history.h"
#include "../click_handler.h"
#include "../key_management.h"
#include "../term_handler.h"
#include "edw.h"


bool gui_resumeHoverInformation(Cursor* cursor, ViewPort* view_port, FileContainer* file, LSP_Hover* hover,
                                int tab_size) {
  assert(hover->is_range);

  LSP_Server* lsp = getLSPServerForLanguage(&lsp_servers, file->lsp_datas.lang_id);
  if (lsp == NULL) {
    return false;
  }

  Cursor start_cur = LSP_tryToReachCursorForLSPPosition(lsp, *cursor, hover->range.pos1);
  Cursor end_cur = LSP_tryToReachCursorForLSPPosition(lsp, *cursor, hover->range.pos2);

  CursorDescriptor start_descriptor = cursor_to_desc(start_cur);
  CursorDescriptor end_descriptor = cursor_to_desc(end_cur);

  fprintf(stderr, "Last position saved (%d, %d), in range (%d, %d) -> (%d, %d)\n",
          view_port->gui->edw_context.lastMousePosition.row, view_port->gui->edw_context.lastMousePosition.column,
          start_descriptor.row, start_descriptor.column, end_descriptor.row, end_descriptor.column);


  // if the last cursor position is not inside the range we don't trigger the popup ! Or if the popup is owned by
  // GOTO_CHOICE
  if (!cursor_desc_is_between(view_port->gui->edw_context.lastMousePosition, start_descriptor, end_descriptor) ||
      view_port->gui->edw_context.pow_owner == GOTO_CHOICE) {
    return false;
  }

  fprintf(stderr, "opening popup !\n");
  // Set focus on the first character of the range (start_cur is before first char, so move right)
  Cursor new_cur = moveRight(start_cur);

  // calculating the best popup size.
  int total_height = 0;
  int global_max_width = 0;
  int max_popup_width = 10000;
  for (int i = 0; i < hover->size; i++) {
    int rows = 0;
    int unused_cols = 0;
    int screen_max_width = max_popup_width - 2;
    countStringFrame(hover->contents[i].value, strlen(hover->contents[i].value), &rows, &unused_cols, &screen_max_width,
                     tab_size);
    total_height += (rows + 1);
    if (screen_max_width > global_max_width) {
      global_max_width = screen_max_width;
    }
    if (i < hover->size - 1) {
      total_height++; // separator
    }
  }

  gui_showGenericPopupWithTextAnchor(view_port, &new_cur, total_height + 2, global_max_width + 2, HOVER_DIAGNOSTICS,
                                     tab_size);
  return true;
}

bool gui_resumeGotoChoice(ViewPort* view_port, Cursor* cursor, int tab_size) {
  gui_showGenericPopupWithTextAnchor(view_port, cursor, 8, 60, GOTO_CHOICE, tab_size);
  return true;
}

bool gui_resumeCompletionTextAnchor(ViewPort* view_port, Cursor* cursor, int tab_size) {
  if ((cursor->file_id.absolute_row == view_port->gui->edw_context.lastTextAnchor.row &&
       cursor->line_id.absolute_column == view_port->gui->edw_context.lastTextAnchor.column &&
       (view_port->gui->edw_context.pow_owner == NO_OWNER ||
        view_port->gui->edw_context.pow_owner == SIGNATURE_HELP)) ||
      view_port->gui->edw_context.pow_owner == COMPLETION) {
    gui_showGenericPopupWithTextAnchor(view_port, cursor, 7, 50, COMPLETION, tab_size);
    return true;
  }
  return false;
}

void gui_showGenericPopupWithTextAnchor(ViewPort* view_port, Cursor* cursor, int height, int width,
                                        PopupOwner popup_owner, int tab_size) {
  return gui_showGenericPopup(view_port->gui, cursor_row(*cursor) - *view_port->screen_y + 1,
                              getScreenXForCursor(*cursor, *view_port->screen_x, tab_size), height, width, popup_owner);
}

void gui_showGenericPopup(gui_Context* gui_context, int y, int x, int prefered_height, int prefered_width,
                          PopupOwner popup_owner) {
  int height = min(getmaxy(gui_context->edw_context.ftw) - y, prefered_height);
  int width = min(getmaxx(gui_context->edw_context.ftw) - x, prefered_width);

  bool isOpened = gui_showEDWPopup(gui_context, y + height, x - 2, height, width, popup_owner);
  // couldn't show popup
  if (!isOpened) {
    return;
  }

  gui_context->edw_context.item_select_offset_y = 0;
  gui_context->edw_context.item_selected = 0;

  wbkgd(gui_context->edw_context.pow, COLOR_PAIR(INFO_COLOR_PAIR));
}

void gui_setLastTextAnchor(gui_Context* gui_context, CursorDescriptor descriptor) {
  gui_context->edw_context.lastTextAnchor = descriptor;
}

void gui_showDiagnostic(gui_Context* gui_context, int y, int x, LSP_Diagnostic* diagnostic, int tab_size) {
  if (diagnostic == NULL) {
    return;
  }

  int height = 3;
  int unused = 0;
  int max_width = getmaxx(gui_context->edw_context.ftw) - 2;

  int ch_length = strlen(diagnostic->message);
  countStringFrame(diagnostic->message, ch_length, &height, &unused, &max_width, tab_size);

  bool isOpened = gui_showEDWPopup(gui_context, y + height, x - 2, height,
                                   min(getmaxx(gui_context->edw_context.ftw), max_width + 2), DIAGNOSTICS);
  // couldn't show popup
  if (!isOpened) {
    return;
  }

  int color = 0;
  switch (diagnostic->severity) {
    case LSP_DIAG_ERROR:
      color = ERROR_COLOR_PAIR;
      break;
    case LSP_DIAG_WARNING:
      color = WARNING_COLOR_PAIR;
      break;
    case LSP_DIAG_INFORMATION:
      color = INFO_COLOR_PAIR;
      break;
    default:
      color = 0;
      break;
  }
  wbkgd(gui_context->edw_context.pow, COLOR_PAIR(color));
  wattr_set(gui_context->edw_context.pow, A_NORMAL, color, NULL);

  box(gui_context->edw_context.pow, 0, 0);

  printToWindow(gui_context->edw_context.pow, diagnostic->message, ch_length, 1, 1, max_width, 0, tab_size);
}


void gui_printCompletionPopup(gui_EDW* context, Cursor* cursor, LSP_ComputedData* lsp_data, int tab_size) {
  if (lsp_data == NULL) {
    fprintf(stderr, "INTERNAL ERROR : Asked to print the popup on a non lsp file.\n");
    return;
  }
  int width = getmaxx(context->pow), height = getmaxy(context->pow);
  werase(context->pow);

  int ca_size = lsp_data->code_actions.size;
  int comp_size = lsp_data->completions.completions.size;
  int total_size = ca_size + comp_size;

  // for the height lines.
  for (int i = 0; i < height; i++) {
    int index = context->item_select_offset_y + i;
    if (index >= total_size) {
      break; // reach the end.
    }

    // setup color for current line.
    if (context->item_selected == index) {
      wattr_set(context->pow, A_NORMAL, WARNING_COLOR_HOVER_PAIR, NULL);
    }
    else {
      wattr_set(context->pow, A_NORMAL, INFO_COLOR_PAIR, NULL);
    }

    if (index < ca_size) {
      // Render Code Action
      LSP_CodeAction* action = &lsp_data->code_actions.items[index];
      char line[width + 10];
      snprintf(line, sizeof(line), "%s %s", action->isPreferred ? "★" : "💡", action->title);
      printToWindow(context->pow, line, -1, 0, i, width, 1, tab_size);
    }
    else {
      // Render Completion
      int comp_idx = index - ca_size;
      wattr_on(context->pow, A_ITALIC, NULL);
      printToWindow(context->pow, trim(lsp_data->completions.completions.items[comp_idx].label), -1, 0, i, width - 1, 1,
                    tab_size);
    }

    // clear attributes for next line
    wattr_set(context->pow, A_NORMAL, INFO_COLOR_PAIR, NULL);
  }
}

const char* gui_getGotoLabel(LSP_GOTO_TYPE type) {
  switch (type) {
    case LSP_GOTO_DEFINITION:
      return "Definitions";
    case LSP_GOTO_DECLARATION:
      return "Declarations";
    case LSP_GOTO_TYPE_DEFINITION:
      return "Type Definitions";
    case LSP_GOTO_IMPLEMENTATION:
      return "Implementations";
    case LSP_FIND_REFERENCE:
      return "References";
    default:
      return "Choices";
  }
}

void gui_printGotoChoicePopup(gui_EDW* context, Cursor* cursor, LSP_ComputedData* lsp_data, int tab_size) {
  int width = getmaxx(context->pow), height = getmaxy(context->pow);
  werase(context->pow);

  wattr_set(context->pow, A_BOLD, INFO_COLOR_PAIR, NULL);

  box(context->pow, 0, 0);
  mvwprintw(context->pow, 0, 1, " Multiple %s found ", gui_getGotoLabel(lsp_data->goto_type));

  for (int i = 0; i < height - 2; i++) {
    int index = context->item_select_offset_y + i;
    if (index >= lsp_data->gotos.size) {
      break;
    }

    if (context->item_selected == index) {
      wattr_set(context->pow, A_NORMAL | A_REVERSE, WARNING_COLOR_HOVER_PAIR, NULL);
    }
    else {
      wattr_set(context->pow, A_NORMAL, INFO_COLOR_PAIR, NULL);
    }

    LSP_Location loc = lsp_data->gotos.items[index];
    char line[PATH_MAX + 30];
    char* filename_copy = strdup(loc.file_name.file_name);
    snprintf(line, sizeof(line), "  📄 %-20s : line %-4d", basename(filename_copy), loc.range.pos1.row + 1);
    free(filename_copy);

    printToWindow(context->pow, line, -1, 1, i + 1, width - 2, 1, tab_size);
  }
}

void gui_printHoverPopup(gui_EDW* context, Cursor* cursor, LSP_ComputedData* lsp_data, int tab_size) {
  int width = getmaxx(context->pow), height = getmaxy(context->pow);
  werase(context->pow);
  wattr_set(context->pow, A_NORMAL, INFO_COLOR_PAIR, NULL);

  box(context->pow, 0, 0);

  int current_y = 1;
  // Hover Information
  for (int i = 0; i < lsp_data->hover.size; i++) {
    if (current_y >= height) {
      break;
    }
    char* text = lsp_data->hover.contents[i].value;
    int len = strlen(text);

    printToWindow(context->pow, text, len, 1, current_y, width - 2, height - current_y - 1, tab_size);

    int lines_consumed = 0;
    int current_column = 0;
    int dummy_width = width;
    countStringFrame(text, len, &lines_consumed, &current_column, &dummy_width, tab_size);

    current_y += (lines_consumed + 1);

    // Add a separator if there are more items
    if (i < lsp_data->hover.size - 1 && current_y < height) {
      mvwhline(context->pow, current_y, 0, ACS_HLINE, width);
      current_y++;
    }
  }
}


void gui_printSignatureHelpPopup(gui_EDW* context, Cursor* cursor, LSP_ComputedData* lsp_data, int tab_size) {
  int width = getmaxx(context->pow), height = getmaxy(context->pow);
  werase(context->pow);

  LSP_SignatureHelp* help = &lsp_data->signature_help;
  if (help->signatures_size == 0) {
    return;
  }

  int sig_idx = help->activeSignature;
  if (sig_idx < 0 || sig_idx >= help->signatures_size) {
    sig_idx = 0;
  }

  LSP_SignatureInformation* sig = &help->signatures[sig_idx];

  // Determine active parameter and highlighting range
  int active_param_idx = sig->activeParameter;
  if (active_param_idx == -1) {
    active_param_idx = help->activeParameter;
  }

  int h_start = -1, h_end = -1;
  if (active_param_idx >= 0 && active_param_idx < sig->parameters_size) {
    LSP_ParameterInformation* param = &sig->parameters[active_param_idx];
    if (param->start != -1) {
      h_start = param->start;
      h_end = param->end;
    }
    else if (param->label[0] != '\0') {
      char* p = strstr(sig->label, param->label);
      if (p) {
        h_start = p - sig->label;
        h_end = h_start + strlen(param->label);
      }
    }
  }

  int label_len = strlen(sig->label);
  int cur_y;

  wattr_set(context->pow, A_NORMAL, WARNING_COLOR_PAIR, NULL);

  // check if we found a parameter to highlight
  if (h_start != -1 && h_end != -1) {
    // if yes we split in 3 part prefix + hightligh part + suffix

    // 1. Prefix
    if (h_start > 0) {
      printToWindow(context->pow, sig->label, h_start, 0, 0, width, 0, tab_size);
    }

    // 2. Highlighted param
    int r_pre = 0, c_pre = 0, w_pre = width;
    countStringFrame(sig->label, h_start, &r_pre, &c_pre, &w_pre, tab_size);

    wattr_set(context->pow, A_NORMAL | A_UNDERLINE | A_ITALIC | A_BOLD, WARNING_COLOR_HOVER_PAIR, NULL);
    printToWindow(context->pow, sig->label + h_start, h_end - h_start, c_pre, r_pre, width, 0, tab_size);
    wattr_set(context->pow, A_NORMAL, WARNING_COLOR_PAIR, NULL);


    // 3. Suffix
    if (h_end < label_len) {
      int r_param = 0, c_param = 0, w_param = width;
      countStringFrame(sig->label, h_end, &r_param, &c_param, &w_param, tab_size);
      printToWindow(context->pow, sig->label + h_end, -1, c_param, r_param, width, 0, tab_size);
    }

    // Calculate final height
    int r_all = 0, c_all = 0, w_all = width;
    countStringFrame(sig->label, label_len, &r_all, &c_all, &w_all, tab_size);
    cur_y = r_all + 1;
  }
  else {
    // No highlight, just print normally
    printToWindow(context->pow, sig->label, -1, 0, 0, width, 0, tab_size);
    int r_all = 0, c_all = 0, w_all = width;
    countStringFrame(sig->label, label_len, &r_all, &c_all, &w_all, tab_size);
    cur_y = r_all + 1;
  }

  wattr_set(context->pow, A_NORMAL, INFO_COLOR_PAIR, NULL);

  if (sig->documentation[0] != '\0') {
    mvwhline(context->pow, cur_y, 0, ACS_HLINE, width);
    printToWindow(context->pow, sig->documentation, -1, 0, cur_y + 1, width, height - (cur_y + 1), tab_size);
  }
}


void gui_printPopup(gui_EDW* context, Cursor* cursor, LSP_ComputedData* lsp_data, int tab_size) {
  switch (context->pow_owner) {
    case COMPLETION:
      gui_printCompletionPopup(context, cursor, lsp_data, tab_size);
      break;
    case HOVER_DIAGNOSTICS:
      gui_printHoverPopup(context, cursor, lsp_data, tab_size);
      break;
    case GOTO_CHOICE:
      gui_printGotoChoicePopup(context, cursor, lsp_data, tab_size);
      break;
    case SIGNATURE_HELP:
      gui_printSignatureHelpPopup(context, cursor, lsp_data, tab_size);
      break;
    default:
      break;
  }
}

bool gui_handleCompletionInput(gui_Context* context, FileContainer* fc, int key,
                               PayloadStateChange* payload_state_change, ModuleContext* payload, MEVENT* m_event) {
  Cursor* cursor = &fc->cursor;
  LSP_ComputedData* lsp_data = fc->lsp_datas.computed;
  History** history_p = &fc->history_frame;
  int height = getmaxy(context->edw_context.pow);

  int ca_size = lsp_data->code_actions.size;
  int comp_size = lsp_data->completions.completions.size;
  int total_size = ca_size + comp_size;

  if (key == H_KEY_MOUSE && m_event != NULL) {
    if (m_event->bstate & BUTTON4_PRESSED) {
      if (context->edw_context.item_select_offset_y > 0) {
        context->edw_context.item_select_offset_y--;
        gui_updateEDW(context);
      }
    }
    else if (m_event->bstate & BUTTON5_PRESSED) {
      if (context->edw_context.item_select_offset_y + height < total_size) {
        context->edw_context.item_select_offset_y++;
        gui_updateEDW(context);
      }
    }
    else if (m_event->bstate & (BUTTON1_PRESSED)) {
      int clicked_item = context->edw_context.item_select_offset_y + (m_event->y - getbegy(context->edw_context.pow));
      if (clicked_item >= 0 && clicked_item < total_size) {
        context->edw_context.item_selected = clicked_item;
        gui_updateEDW(context);
      }
    }

    if (m_event->bstate & BUTTON1_CLICKED) {
      int clicked_item = context->edw_context.item_select_offset_y + (m_event->y - getbegy(context->edw_context.pow));
      if (clicked_item >= 0 && clicked_item < total_size) {
        if (clicked_item < ca_size) {
          executeCodeAction(fc, cursor, &lsp_data->code_actions.items[clicked_item], payload);
        }
        else {
          LSP_Server* lsp = getLSPServerForLanguage(&lsp_servers, fc->lsp_datas.lang_id);
          executeLSPCompletion(lsp, cursor, lsp_data->completions.completions.items + (clicked_item - ca_size),
                               history_p, payload_state_change, LF_tab(fc->feature));
        }
        gui_closeEDWPopup(context);
      }
    }

    if (m_event->bstate & NO_EVENT_MOUSE) {
      int clicked_item = context->edw_context.item_select_offset_y + (m_event->y - getbegy(context->edw_context.pow));
      if (clicked_item >= 0 && clicked_item < total_size && clicked_item != context->edw_context.item_selected) {
        context->edw_context.item_selected = clicked_item;
        gui_updateEDW(context);
      }
    }

    return true;
  }

  switch (key) {
    case H_KEY_UP:
      if (context->edw_context.item_selected > 0) {
        context->edw_context.item_selected--;
      }
      if (context->edw_context.item_selected < context->edw_context.item_select_offset_y) {
        context->edw_context.item_select_offset_y = context->edw_context.item_selected;
      }
      gui_updateEDW(context);
      return true;
    case H_KEY_DOWN:
      context->edw_context.item_selected++;
      if (context->edw_context.item_selected >= total_size) {
        context->edw_context.item_selected = total_size > 0 ? total_size - 1 : 0;
      }
      if (context->edw_context.item_selected >= context->edw_context.item_select_offset_y + height) {
        context->edw_context.item_select_offset_y++;
      }
      gui_updateEDW(context);
      return true;
    case H_KEY_ENTER:
    case H_KEY_TAB:
      if (context->edw_context.item_selected < total_size) {
        if (context->edw_context.item_selected < ca_size) {
          executeCodeAction(fc, cursor, &lsp_data->code_actions.items[context->edw_context.item_selected], payload);
        }
        else {
          LSP_Server* lsp = getLSPServerForLanguage(&lsp_servers, fc->lsp_datas.lang_id);
          executeLSPCompletion(lsp, cursor,
                               lsp_data->completions.completions.items + (context->edw_context.item_selected - ca_size),
                               history_p, payload_state_change, LF_tab(fc->feature));
        }
      }
      gui_closeEDWPopup(context);
      return true;
    default:
      break;
  }

  return false;
}

bool gui_handleGotoChoiceInput(gui_Context* context, FileContainer* fc, int key,
                               PayloadStateChange* payload_state_change, ModuleContext* payload, MEVENT* m_event) {
  LSP_ComputedData* lsp_data = fc->lsp_datas.computed;
  int height = getmaxy(context->edw_context.pow) - 2;

  if (key == H_KEY_MOUSE && m_event != NULL) {
    int item_count = lsp_data->gotos.size;
    int items_per_page = height;

    if (m_event->bstate & BUTTON4_PRESSED) {
      if (context->edw_context.item_select_offset_y > 0) {
        context->edw_context.item_select_offset_y--;
        gui_updateEDW(context);
      }
    }
    else if (m_event->bstate & BUTTON5_PRESSED) {
      if (context->edw_context.item_select_offset_y + items_per_page < item_count) {
        context->edw_context.item_select_offset_y++;
        gui_updateEDW(context);
      }
    }
    else if (m_event->bstate) {
      int clicked_item =
        context->edw_context.item_select_offset_y + (m_event->y - getbegy(context->edw_context.pow) - 1);
      if (clicked_item >= 0 && clicked_item < item_count && context->edw_context.item_selected != clicked_item) {
        context->edw_context.item_selected = clicked_item;
        gui_updateEDW(context);
      }
    }

    if (m_event->bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED)) {
      int clicked_item =
        context->edw_context.item_select_offset_y + (m_event->y - getbegy(context->edw_context.pow) - 1);
      if (clicked_item >= 0 && clicked_item < item_count) {
        jumpToLocation(payload, lsp_data->gotos.items[clicked_item]);
        gui_closeEDWPopup(context);
      }
    }
    return true;
  }

  switch (key) {
    case H_KEY_UP:
      if (context->edw_context.item_selected > 0) {
        context->edw_context.item_selected--;
      }
      if (context->edw_context.item_selected < context->edw_context.item_select_offset_y) {
        context->edw_context.item_select_offset_y = context->edw_context.item_selected;
      }
      gui_updateEDW(context);
      return true;
    case H_KEY_DOWN:
      context->edw_context.item_selected++;
      if (context->edw_context.item_selected >= lsp_data->gotos.size) {
        context->edw_context.item_selected = lsp_data->gotos.size - 1;
      }
      if (context->edw_context.item_selected >= context->edw_context.item_select_offset_y + height) {
        context->edw_context.item_select_offset_y++;
      }
      gui_updateEDW(context);
      return true;
    case H_KEY_ENTER:
    case H_KEY_TAB:
      if (context->edw_context.item_selected < lsp_data->gotos.size && payload != NULL) {
        jumpToLocation(payload, lsp_data->gotos.items[context->edw_context.item_selected]);
      }
      gui_closeEDWPopup(context);
      return true;
    default:
      break;
  }

  return false;
}


bool gui_handlePopupInput(gui_Context* context, FileContainer* fc, int key, PayloadStateChange* payload_state_change,
                          ModuleContext* payload, MEVENT* m_event) {
  if (context->edw_context.show_pow == false || context->edw_context.pow == NULL) {
    return false;
  }

  if (key == H_KEY_MOUSE && m_event != NULL && !isClickInsideWindow(context->edw_context.pow, m_event)) {
    return false;
  }

  switch (context->edw_context.pow_owner) {
    case COMPLETION:
      return gui_handleCompletionInput(context, fc, key, payload_state_change, payload, m_event);
    case GOTO_CHOICE:
      return gui_handleGotoChoiceInput(context, fc, key, payload_state_change, payload, m_event);
    case SIGNATURE_HELP:
      return false; // Let the editor handle typing, which will trigger updates
    default:
      break;
  }
  return false;
}
