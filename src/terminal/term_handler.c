#include <assert.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../data-management/file_management.h"
#include "kitty_protocol.h"
#include "term_handler.h"

#include <limits.h>
#include <math.h>

#include "../core/editor_context.h"
#include "../environnement/constants.h"
#include "highlight.h"
#include "windows/edw.h"
#include "windows/few.h"
#include "windows/ofw.h"
#include "windows/tpw.h"


////// -------------- WINDOWS MANAGEMENTS --------------


void gui_initGUIContext(gui_Context* gui_context) {
  gui_context->focus_w = NULL; // Used to set the window where start mouse drag
  gui_context->focused_panel = PANEL_EDITOR;
  gui_context->toplevel_popups = NULL;
  gui_context->active_notifications = NULL;
  gui_context->notification_count = 0;

  gui_initEDWContext(&gui_context->edw_context);
  gui_initFEWContext(&gui_context->few_context);
  gui_initOFWContext(&gui_context->ofw_context);
}

void gui_initNCurses(gui_Context* gui_context) {
  set_escdelay(25);
  // Init ncurses
  if (initscr() == NULL) {
    fprintf(stderr, "Error: Could not initialize ncurses. Check your terminal settings.\n");
    exit(EXIT_FAILURE);
  }
  gui_resizeFEW(gui_context, -1);
  gui_resizeEDW(gui_context, -1);
  gui_resizeOFW(gui_context);
  // Keyboard setup
  raw();
  keypad(stdscr, TRUE);
  noecho();
  curs_set(0);
  // Mouse setup
  mousemask(0, NULL);
  keyok(KEY_MOUSE, FALSE);
  timeout(TIME_OUT_DELAY);
  printf("\033[?1003h\033[?1006h\033[5 q"); // enable mouse tracking (including SGR 1006) and beam cursor
  kitty_enable();                           // Enable Kitty Keyboard Protocol (flags 1 and 2)
  fflush(stdout);
  // Color setup
  if (has_colors()) {
    start_color();
    use_default_colors();

    // Check if we can change colors. If not, we'll stick to default palette.
    if (can_change_color()) {
      init_extended_color(BG_COLOR_DEFAULT, 50, 50, 50);
      init_extended_color(BG_COLOR_HOVER, 200, 200, 200);
      init_extended_color(BG_COLOR_POPUP, 100, 100, 100);
      init_extended_color(COLOR_CYAN, 100, 700, 650);
      init_extended_color(COLOR_TRUE_WHITE, 1000, 1000, 1000);
    }

    init_extended_pair(DEFAULT_COLOR_PAIR, COLOR_WHITE, BG_COLOR_DEFAULT);
    init_extended_pair(DEFAULT_COLOR_HOVER_PAIR, COLOR_WHITE, BG_COLOR_HOVER);

    init_extended_pair(ERROR_COLOR_PAIR, COLOR_RED, BG_COLOR_POPUP);
    init_extended_pair(ERROR_COLOR_HOVER_PAIR, COLOR_RED, BG_COLOR_HOVER);

    init_extended_pair(WARNING_COLOR_PAIR, COLOR_YELLOW, BG_COLOR_POPUP);
    init_extended_pair(WARNING_COLOR_HOVER_PAIR, COLOR_YELLOW, BG_COLOR_HOVER);

    init_extended_pair(INFO_COLOR_PAIR, COLOR_CYAN, BG_COLOR_POPUP);
    init_extended_pair(INFO_COLOR_HOVER_PAIR, COLOR_CYAN, BG_COLOR_HOVER);

    init_extended_pair(STATUS_BAR_COLOR_PAIR, COLOR_TRUE_WHITE, BG_COLOR_HOVER);
  }
}

void gui_setFocus(gui_Context* gui_context, WINDOW* w) { gui_context->focus_w = w; }

void gui_resetFocus(gui_Context* gui_context) { gui_context->focus_w = NULL; }

////// -------------- PRINT FUNCTIONS --------------


void gui_repaintGUI(gui_Context* gui_context, WindowHighlightDescriptor* highlight_descriptor, ExplorerFolder* explorer,
                    FileContainer* files, int file_count, int current_file) {
  wnoutrefresh(stdscr);
  gui_repaintEDW(&gui_context->edw_context, files + current_file, highlight_descriptor);
  gui_repaintFEW(&gui_context->few_context, explorer, gui_context->focused_panel == PANEL_FILE_EXPLORER);
  gui_repaintOFW(&gui_context->ofw_context, files, file_count, current_file);
  gui_repaintTPW(gui_context);
  doupdate();
}


LineMarker gui_getMarkerForCurrentLine(int row, WindowHighlightDescriptor* highlight_descriptor, int whd_offset,
                                       void** diagnostic) {
  LineMarker marker = LINE_MARKER_NONE;
  if (diagnostic != NULL) {
    *diagnostic = NULL;
  }

  while (whd_offset < highlight_descriptor->size &&
         highlight_descriptor->descriptors[whd_offset].begin.abs_row <= row) {

    if (highlight_descriptor->descriptors[whd_offset].end.abs_row >= row) {
      if (highlight_descriptor->descriptors[whd_offset].line_marker != 0) {
        if (highlight_descriptor->descriptors[whd_offset].line_marker < marker || marker == 0) {
          marker = highlight_descriptor->descriptors[whd_offset].line_marker;
          if (diagnostic != NULL) {
            *diagnostic = highlight_descriptor->descriptors[whd_offset].diagnostic;
          }
        }
      }
    }
    whd_offset++;
  }

  return marker;
}

void gui_updateEDW(gui_Context* gui_context) { gui_context->edw_context.refresh_edw = true; }

void gui_updateFEW(gui_Context* gui_context) { gui_context->few_context.refresh_few = true; }

void gui_updateOFW(gui_Context* gui_context) { gui_context->ofw_context.refresh_ofw = true; }

void gui_updateGUI(gui_Context* gui_context) {
  gui_updateEDW(gui_context);
  gui_updateFEW(gui_context);
  gui_updateOFW(gui_context);
  gui_updateTPW(gui_context);
}

bool gui_doesGUINeedRepaint(gui_Context* gui_context) {
  return gui_context->edw_context.refresh_edw || gui_context->few_context.refresh_few ||
         gui_context->ofw_context.refresh_ofw || gui_context->refresh_tpw;
}

////// -------------- UTILS FUNCTIONS --------------


void moveScreenToMatchCursor(gui_Context* context, Cursor cursor, int* screen_x, int* screen_y, int tab_size) {
  int current_lines = getmaxy(context->edw_context.ftw);
  int current_columns = getmaxx(context->edw_context.ftw);

  if (cursor.file_id.absolute_row - (*screen_y + current_lines) + 1 >= 0) {
    *screen_y = cursor.file_id.absolute_row - current_lines + 2;
    if (*screen_y < 1) {
      *screen_y = 1;
    }
  }
  else if (cursor.file_id.absolute_row < *screen_y + 1) {
    *screen_y = cursor.file_id.absolute_row - 1;
    if (*screen_y < 1) {
      *screen_y = 1;
    }
  }

  int screen_x_wide_char = getScreenXForCursor(cursor, *screen_x, tab_size) + *screen_x;
  if (screen_x_wide_char - (*screen_x + current_columns - 8) >= 0) {
    *screen_x = screen_x_wide_char - current_columns + 8;
    if (*screen_x < 1) {
      *screen_x = 1;
    }
  }
  else if (screen_x_wide_char - 5 < *screen_x) {
    *screen_x = screen_x_wide_char - 5;
    if (*screen_x < 1) {
      *screen_x = 1;
    }
  }
}

void centerCursorOnScreen(gui_Context* context, Cursor cursor, int* screen_x, int* screen_y, int tab_size) {
  // center for y, but right for x.
  *screen_x = cursor.line_id.absolute_column - (COLS /*/ 2*/);
  *screen_y = cursor.file_id.absolute_row - (LINES / 2);

  if (*screen_x < 1) {
    *screen_x = 1;
  }
  if (*screen_y < 1) {
    *screen_y = 1;
  }

  // To match right for x.
  moveScreenToMatchCursor(context, cursor, screen_x, screen_y, tab_size);
}


int getScreenXForCursor(Cursor cursor, int screen_x, int tab_size) {
  Cursor initial = cursor;
  Cursor old_cursor = cursor;
  int atAdd = 0;
  int size;


  if (cursor.line_id.absolute_column != 0 &&
      (size = charPrintSize(getCharForLineIdentifier(cursor.line_id), tab_size)) >= 2) {
    atAdd += size - 1;
  }
  cursor = moveLeft(cursor);


  while (screen_x <= cursor.line_id.absolute_column && cursor_eq(cursor, old_cursor) == false &&
         cursor.file_id.absolute_row == old_cursor.file_id.absolute_row) {
    assert(cursor.line_id.absolute_column != 0);
    Char_U8 current_ch = getCharForLineIdentifier(cursor.line_id);
    if ((size = charPrintSize(current_ch, tab_size)) >= 2) {
      atAdd += size - 1;
    }

    old_cursor = cursor;
    cursor = moveLeft(cursor);
  }

  return initial.line_id.absolute_column - screen_x + 1 + atAdd;
}

LineIdentifier getLineIdForScreenX(LineIdentifier line_id, int screen_x, int x_click, int tab_size) {
  line_id = tryToReachAbsColumn(line_id, screen_x - 1);

  int current_column = 0;
  int x_el = 0;

  while (hasElementAfterLine(line_id) == true && current_column <= x_click) {
    line_id = tryToReachAbsColumn(line_id, line_id.absolute_column + 1);
    int size = charPrintSize(getCharForLineIdentifier(line_id), tab_size);
    if (size <= 0) {
      size = 1; // TODO handle non UTF_8 char.
    }
    current_column += size;
    x_el++;
  }

  if (x_click >= current_column) {
    x_el++;
  }

  return tryToReachAbsColumn(line_id, screen_x + x_el - 2);
}


void setDesiredColumn(Cursor cursor, int* desired_column) { *desired_column = cursor.line_id.absolute_column; }
