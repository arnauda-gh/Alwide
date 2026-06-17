#include "edw.h"

#include <assert.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ttydefaults.h>
#include <unistd.h>

#include "../../core/editor_context.h"
#include "../../data-management/file_management.h"
#include "../../environnement/global_variables.h"
#include "../../utils/tools.h"
#include "../click_handler.h"
#include "../highlight.h"
#include "../key_management.h"
#include "../term_handler.h"
#include "gui_entities.h"
#include "pow.h"

void gui_initEDWContext(gui_EDW* context) {
  context->ftw = NULL; // File Text Window
  context->lnw = NULL; // Line Number Window
  context->pow = NULL; // Popup Window
  context->sbw = NULL; // Status Bar Window

  context->refresh_edw = true; // Need to reprint editor window
  context->show_sbw = true;    // Status bar visible by default
  context->length_line_number = 0;

  // popup init values
  context->show_pow = false;
  context->pow_owner = NO_OWNER;
  context->item_select_offset_y = 0;
  context->item_selected = 0;
  context->lastTextAnchor.row = 0;
  context->lastTextAnchor.column = 0;
  context->lastMousePosition.row = 0;
  context->lastMousePosition.column = 0;
}


void gui_resizeEDW(gui_Context* gui_context, int length_line_number) {
  gui_EDW* context = &gui_context->edw_context;

  if (length_line_number != -1) {
    context->length_line_number = length_line_number;
  }

  delwin(context->ftw);
  delwin(context->lnw);
  delwin(context->sbw);

  int height = LINES - gui_context->ofw_context.ofw_height;
  if (height < 1) {
    height = 1;
  }

  context->ftw = newwin(height, 0, gui_context->ofw_context.ofw_height,
                        context->length_line_number + 1 + gui_context->few_context.few_width);
  context->lnw = newwin(height, context->length_line_number + 1, gui_context->ofw_context.ofw_height,
                        gui_context->few_context.few_width);

  if (context->show_sbw) {
    context->sbw = newwin(1, getmaxx(context->ftw) + getmaxx(context->lnw), LINES - 1, getbegx(context->lnw));
    wbkgd(context->sbw, COLOR_PAIR(DEFAULT_COLOR_PAIR));
  }
  else {
    context->sbw = NULL;
  }

  wbkgd(context->ftw, COLOR_PAIR(DEFAULT_COLOR_PAIR));
  context->refresh_edw = true;
}


void printEditor_printLineNumber(gui_EDW* context, Cursor cursor, int screen_y, FileIdentifier file_cur, int row,
                                 WindowHighlightDescriptor* highlight_descriptor, int whd_offset) {
  char line_number[40];
  sprintf(line_number, "%d", file_cur.absolute_row);
  int lineNumberSize = strlen(line_number);

  NCURSES_PAIRS_T color = DEFAULT_COLOR_PAIR;
  attr_t attr = A_BOLD | A_ITALIC;


  LineMarker marker = gui_getMarkerForCurrentLine(row, highlight_descriptor, whd_offset, NULL);
  switch (marker) {
    case LSP_ERROR:
      color = ERROR_COLOR_PAIR;
      break;
    case LSP_WARNING:
      color = WARNING_COLOR_PAIR;
      break;
    case LSP_INFORMATION:
      color = INFO_COLOR_PAIR;
      break;
    default:
      attr = A_DIM;
      break;
  }

  wattr_set(context->lnw, attr, color, NULL);

  if (file_cur.absolute_row == cursor.file_id.absolute_row) {
    attr = A_BOLD;
    wattroff(context->lnw, A_DIM);
    wattron(context->lnw, A_BOLD);
  }
  else {
    attr = A_DIM;
  }

  wmove(context->lnw, row - screen_y, 0);
  for (int i = 0; i < getmaxx(context->lnw) - lineNumberSize - 1; i++) {
    wprintw(context->lnw, " ");
  }
  wprintw(context->lnw, "%s", line_number);
  wattr_set(context->lnw, attr, DEFAULT_COLOR_PAIR, NULL);
  wprintw(context->lnw, "│");
}


void printEditor_printFileContent(gui_EDW* context, Cursor cursor, Cursor select_cursor, int screen_x,
                                  WindowHighlightDescriptor* highlight_descriptor, FileIdentifier file_cur,
                                  const int column_count, int* whd_offset, int tab_size) {
  LineIdentifier begin_screen_line_cur =
    tryToReachAbsColumn(moduloLineIdentifierR(getLineForFileIdentifier(file_cur), 0), screen_x);
  LineIdentifier end_screen_line_cur = tryToReachAbsColumn(begin_screen_line_cur, screen_x + column_count - 3);

  int column = screen_x;
  while (end_screen_line_cur.absolute_column >= screen_x &&
         begin_screen_line_cur.absolute_column <= end_screen_line_cur.absolute_column &&
         screen_x + column_count - 3 >= column) {
    Char_U8 ch = getCharForLineIdentifier(begin_screen_line_cur);
    Cursor ch_cursor = cursorOf(file_cur, begin_screen_line_cur);

    int size = charPrintSize(ch, tab_size);
    // If the char is detected as not printable char.
    if (size <= 0) {
      ch = readChar_U8FromCharArray("�");
      size = 1;
    }

    // If a char of size 2 is at the end of the line replace it by '_' to avoid line overflow.
    if (size >= 2 && screen_x + column_count - 4 < column) {
      ch = readChar_U8FromCharArray("_");
      // size = 1;
    }

    // determine if the char is selected or not.
    bool selected_style =
      cursor_is_disabled(select_cursor) == false && cursor_is_inside(ch_cursor, select_cursor, cursor);
    // get current highlight.
    TextPartHighlightDescriptor* current_highlight =
      whd_tphd_forCursorWithOffsetIndex(highlight_descriptor, ch_cursor, whd_offset);


    // default style
    attr_t attr = A_NORMAL;
    NCURSES_PAIRS_T color = DEFAULT_COLOR_PAIR;

    // override default style if a textPartHighlight was found.
    if (current_highlight != NULL) {
      attr = current_highlight->attributes;
      color = current_highlight->color;
    }

    // add the offset if the current text is selected
    if (selected_style) {
      color += COLOR_HOVER_OFFSET;
    }

    // setting the char attribute.
    wattr_set(context->ftw, attr, color, 0);

    if (ch.t[0] == '\t') {
      Char_U8 space = readChar_U8FromInput(' ');
      for (int i = 0; i < tab_size; i++) {
        gui_printChar_U8ToNcurses(context->ftw, space);
      }
    }
    else {
      gui_printChar_U8ToNcurses(context->ftw, ch);
    }

    // move to next column
    begin_screen_line_cur.relative_column++;
    begin_screen_line_cur.absolute_column++;
    column += size;
  }

  // show empty line selected.
  if (begin_screen_line_cur.absolute_column == end_screen_line_cur.absolute_column &&
      hasElementAfterLine(end_screen_line_cur) == false) {
    if (cursor_is_disabled(select_cursor) == false &&
        cursor_is_inside(cursorOf(file_cur, begin_screen_line_cur), select_cursor, cursor)) {

      // if line selected
      wattr_set(context->ftw, A_NORMAL, DEFAULT_COLOR_HOVER_PAIR, 0);
      wprintw(context->ftw, " ");
      wattr_set(context->ftw, A_NORMAL, 0, NULL);
    }
  }

  // If the line is not fully display show '>'
  if (hasElementAfterLine(end_screen_line_cur)) {
    wattr_set(context->ftw, A_BOLD | A_UNDERLINE | A_DIM, DEFAULT_COLOR_PAIR, 0);
    wprintw(context->ftw, ">");
  }

  wprintw(context->ftw, "\n");
}


void printEditor_printCursor(gui_EDW* context, Cursor cursor, int screen_x, int screen_y,
                             WindowHighlightDescriptor* highlight_descriptor, const int line_count,
                             const int column_count, int tab_size) {
  // Check if cursor is in the screen and print it if needed.
  if (cursor.file_id.absolute_row >= screen_y && cursor.file_id.absolute_row < screen_y + line_count &&
      cursor.line_id.absolute_column >= screen_x - 1 && cursor.line_id.absolute_column <= screen_x + column_count - 3) {
    int x = getScreenXForCursor(cursor, screen_x, tab_size);
    wmove(context->ftw, cursor.file_id.absolute_row - screen_y, x);

    TextPartHighlightDescriptor* current_highlight = NULL;

    char size = 1;
    if (hasElementAfterLine(cursor.line_id) == true) {
      // Check the size of the the char which is under cursor.
      Cursor tmp = moveRight(cursor);
      size = charPrintSize(getCharForLineIdentifier(tmp.line_id), tab_size);
      int unused = 0;
      current_highlight = whd_tphd_forCursorWithOffsetIndex(highlight_descriptor, tmp, &unused);
    }
    else {
      int unused = 0;
      current_highlight = whd_tphd_forCursorWithOffsetIndex(highlight_descriptor, cursor, &unused);
    }


    attr_t attr = A_STANDOUT;
    NCURSES_PAIRS_T color = DEFAULT_COLOR_PAIR;

    if (current_highlight != NULL) {
      attr |= current_highlight->attributes;
      color = current_highlight->color;
    }

    wchgat(context->ftw, size, attr, color, NULL);
  }
}


void move_physical_cursor(gui_EDW* context, Cursor cursor, int screen_x, int screen_y, const int line_count,
                          const int column_count, int tab_size) {
  int ftw_x = getScreenXForCursor(cursor, screen_x, tab_size);
  if (cursor.file_id.absolute_row >= screen_y && cursor.file_id.absolute_row < screen_y + line_count && ftw_x >= 0 &&
      ftw_x <= getmaxx(context->ftw) - 3) {
    curs_set(1);
    int abs_y = cursor.file_id.absolute_row - screen_y + getbegy(context->ftw);
    int abs_x = ftw_x + getbegx(context->ftw);
    move(abs_y, abs_x);
  }
  else {
    curs_set(0);
  }
}


void gui_repaintEDW(gui_EDW* context, FileContainer* fc, WindowHighlightDescriptor* highlight_descriptor) {
  if (!context->refresh_edw) {
    return;
  }
  fprintf(stderr, "print EDW\n");
  wmove(context->ftw, 0, 0);
  FileIdentifier file_cur = fc->cursor.file_id;

  const int line_count = getmaxy(context->ftw);
  const int column_count = getmaxx(context->ftw);


  // ===============  FOR EACH LINE  ===============
  int whd_offset = 0;
  for (int row = fc->screen_y; row < fc->screen_y + line_count; row++) {
    // getting the row to print.
    file_cur = tryToReachAbsRow(file_cur, row);

    // if the row is couldn't be reached skip it.
    if (file_cur.absolute_row != row) {
      // show empty line number.
      wmove(context->lnw, row - fc->screen_y, 0);
      for (int i = 0; i < getmaxx(context->lnw); i++) {
        wprintw(context->lnw, " ");
      }

      // show ~ as content in file text window
      wattr_set(context->ftw, A_NORMAL, DEFAULT_COLOR_PAIR, NULL);
      wprintw(context->ftw, "~\n");

      continue;
    }

    // ===============  Print line number  ===============
    printEditor_printLineNumber(context, fc->cursor, fc->screen_y, file_cur, row, highlight_descriptor, whd_offset);


    // ===============  Print File Content  ===============
    printEditor_printFileContent(context, fc->cursor, fc->select_cursor, fc->screen_x, highlight_descriptor, file_cur,
                                 column_count, &whd_offset, LF_tab_size(fc->feature));
  }

  // ===============  Print Cursor  ===============
#ifdef SIMULATED_CURSOR
  printEditor_printCursor(context, fc->cursor, fc->screen_x, fc->screen_y, highlight_descriptor, line_count,
                          column_count, LF_tab_size(fc->feature));
#else
  move_physical_cursor(context, cursor, screen_x, screen_y, line_count, column_count, tab_size);
#endif

  wnoutrefresh(context->lnw);
  wnoutrefresh(context->ftw);

  if (context->show_pow) {
    assert(context->pow != NULL);
    gui_printPopup(context, &fc->cursor, fc->lsp_datas.computed, LF_tab_size(fc->feature));
    wnoutrefresh(context->pow);
  }

  gui_repaintSBW(context, fc);

  context->refresh_edw = false;
}

int getEDW_LengthLineNumber(gui_Context* gui_context) { return gui_context->edw_context.length_line_number; }

bool gui_showEDWPopup(gui_Context* gui_context, int y, int x, int height, int width, PopupOwner owner) {
  delwin(gui_context->edw_context.pow);
  gui_context->edw_context.pow = newwin(height, width, y - height + getbegy(gui_context->edw_context.ftw),
                                        x + getbegx(gui_context->edw_context.ftw) + 2);

  gui_context->edw_context.show_pow = gui_context->edw_context.pow != NULL;
  if (gui_context->edw_context.show_pow) {
    gui_context->edw_context.pow_owner = owner;
  }
  gui_context->edw_context.refresh_edw = true;

  return gui_context->edw_context.show_pow;
}

bool gui_adaptEDWPopup(gui_Context* gui_context, int slice_x, int slice_y) {
  if (gui_context->edw_context.pow == NULL) {
    return false;
  }

  switch (gui_context->edw_context.pow_owner) {
    case NO_OWNER:
      // TO change with the value that you don't want to follow with scroll.
      return false;
    case DIAGNOSTICS:
      slice_x = 0;
    default:
      break;
  }

  if (slice_x == 0 && slice_y == 0) {
    return true;
  }

  const int x = getbegx(gui_context->edw_context.pow) - slice_x;
  const int y = getbegy(gui_context->edw_context.pow) - slice_y;
  const int width = getmaxx(gui_context->edw_context.pow);
  const int height = getmaxy(gui_context->edw_context.pow);

  delwin(gui_context->edw_context.pow);
  gui_context->edw_context.pow = newwin(height, width, y, x);

  gui_context->edw_context.show_pow = gui_context->edw_context.pow != NULL;
  if (gui_context->edw_context.pow) {
    gui_updateEDW(gui_context);
  }
  else {
    gui_context->edw_context.pow_owner = NO_OWNER;
  }

  return gui_context->edw_context.show_pow;
}

void gui_closeEDWPopup(gui_Context* gui_context) {
  gui_context->edw_context.show_pow = false;
  gui_context->edw_context.pow_owner = NO_OWNER;
  gui_updateEDW(gui_context);
}

void gui_switchSBW(gui_Context* gui_context) {
  gui_context->edw_context.show_sbw = !gui_context->edw_context.show_sbw;
  gui_resizeEDW(gui_context, -1);
}

void gui_repaintSBW(gui_EDW* context, FileContainer* fc) {
  if (context->sbw == NULL || !context->show_sbw || !fc) {
    return;
  }

  // Format left-aligned string: Path from CWD with smart truncation
  char left_str[256];
  if (fc->io_file.status == NONE || strlen(fc->io_file.path_abs) == 0) {
    snprintf(left_str, sizeof(left_str), " [New File]");
  }
  else {
    const char* path_to_use = fc->io_file.path_abs;
    char path_display[256];
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      int cwd_len = strlen(cwd);
      if (strncmp(path_to_use, cwd, cwd_len) == 0) {
        const char* rel_path = path_to_use + cwd_len;
        if (*rel_path == '/') {
          rel_path++;
        }
        strncpy(path_display, rel_path, sizeof(path_display));
        path_display[sizeof(path_display) - 1] = '\0';
      }
      else {
        strncpy(path_display, path_to_use, sizeof(path_display));
        path_display[sizeof(path_display) - 1] = '\0';
      }
    }
    else {
      strncpy(path_display, path_to_use, sizeof(path_display));
      path_display[sizeof(path_display) - 1] = '\0';
    }

    // Truncate left-aligned path if it exceeds 45 characters
    int max_path_len = 45;
    int path_len = strlen(path_display);
    if (path_len > max_path_len) {
      char temp[256];
      int cut_index = path_len - max_path_len + 3; // +3 for "..."
      const char* slash = strchr(path_display + cut_index, '/');
      if (slash != NULL) {
        snprintf(temp, sizeof(temp), "...%s", slash);
      }
      else {
        snprintf(temp, sizeof(temp), "...%s", path_display + cut_index);
      }
      strncpy(path_display, temp, sizeof(path_display));
      path_display[sizeof(path_display) - 1] = '\0';
    }
    snprintf(left_str, sizeof(left_str), " %s", path_display);
  }

  // Format right-aligned string elements
  const char* lang = LF_label(fc->feature);
  int tab_size = LF_tab_size(fc->feature);
  bool use_space = LF_tab_use_space(fc->feature);

  // Sum byte counts across all FileNode blocks
  int total_bytes = 0;
  FileNode* fn = fc->root;
  while (fn != NULL) {
    total_bytes += fn->byte_count;
    fn = fn->next;
  }

  char size_str[32];
  if (total_bytes < 1024) {
    snprintf(size_str, sizeof(size_str), "%d B", total_bytes);
  }
  else if (total_bytes < 1024 * 1024) {
    snprintf(size_str, sizeof(size_str), "%.1f KB", total_bytes / 1024.0);
  }
  else {
    snprintf(size_str, sizeof(size_str), "%.1f MB", total_bytes / (1024.0 * 1024.0));
  }

  int total_lines = sizeFileNode(fc->root);

  char status_str[512];
  snprintf(status_str, sizeof(status_str), "%s  •  %d %s  •  %s  • %4d lines  • %4d:%-3d", lang ? lang : "Plain Text",
           tab_size, use_space ? "sp" : "tb", size_str, total_lines, cursor_row(fc->cursor), cursor_col(fc->cursor));

  // Use countStringFrame to get the real screen column size of the right-aligned status string
  int rows = 0;
  int cols = 0;
  countStringFrame(status_str, strlen(status_str), &rows, &cols, NULL, tab_size);
  int str_len = cols;

  int width = getmaxx(context->ftw) + getmaxx(context->lnw);

  werase(context->sbw);
  wattron(context->sbw, COLOR_PAIR(STATUS_BAR_COLOR_PAIR));

  // fill the window with space with style.
  wmove(context->sbw, 0, 0);
  whline(context->sbw, ' ', width);

  // Print left-aligned text (file name)
  mvwprintw(context->sbw, 0, 2, "%s%s", left_str, isFileEdited(fc) ? " *" : "");

  // print the right-aligned text
  wmove(context->sbw, 0, width - str_len);
  waddstr(context->sbw, status_str);

  wattroff(context->sbw, COLOR_PAIR(STATUS_BAR_COLOR_PAIR));

  wnoutrefresh(context->sbw);
}
