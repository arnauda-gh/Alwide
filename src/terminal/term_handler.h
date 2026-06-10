#ifndef NCURSES_HANDLER_H
#define NCURSES_HANDLER_H
#include <ncurses.h>
#include <wchar.h>

#include "../data-management/file_management.h"
#include "../data-management/file_structure.h"
#include "../io-management/io_explorer.h"
#include "highlight.h"
#include "windows/gui_entities.h"

#define TIME_OUT_DELAY 20


////// -------------- WINDOWS MANAGEMENTS --------------

void gui_initGUIContext(gui_Context* gui_context);

void gui_initNCurses(gui_Context* gui_context);

void gui_setFocus(gui_Context* gui_context, WINDOW* w);

void gui_resetFocus(gui_Context* gui_context);

////// -------------- PRINT FUNCTIONS --------------

void gui_repaintGUI(gui_Context* gui_context, WindowHighlightDescriptor* highlight_descriptor, ExplorerFolder* explorer,
                    FileContainer* files, int file_count, int current_file);


LineMarker gui_getMarkerForCurrentLine(int row, WindowHighlightDescriptor* highlight_descriptor, int whd_offset,
                                       void** diagnostic);

void gui_updateEDW(gui_Context* gui_context);

void gui_updateFEW(gui_Context* gui_context);

void gui_updateOFW(gui_Context* gui_context);

void gui_updateGUI(gui_Context* gui_context);

bool gui_doesGUINeedRepaint(gui_Context* gui_context);

////// -------------- UTILS FUNCTIONS --------------

void moveScreenToMatchCursor(gui_Context* context, Cursor cursor, int* screen_x, int* screen_y, int tab_size);

void centerCursorOnScreen(gui_Context* context, Cursor cursor, int* screen_x, int* screen_y, int tab_size);

int getScreenXForCursor(Cursor cursor, int screen_x, int tab_size);

LineIdentifier getLineIdForScreenX(LineIdentifier line_id, int screen_x, int x_click, int tab_size);

void setDesiredColumn(Cursor cursor, int* desired_column);



#endif // NCURSES_HANDLER_H
