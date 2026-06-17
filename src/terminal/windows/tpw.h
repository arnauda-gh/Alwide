#ifndef ALWIDE_TOPLEVEL_POPUP_H
#define ALWIDE_TOPLEVEL_POPUP_H

#include "gui_entities.h"

typedef enum {
  GUI_TPW_POS_CUSTOM,
  GUI_TPW_POS_CENTER,
  GUI_TPW_POS_BOTTOM,
  GUI_TPW_POS_BOTTOM_RIGHT,
  GUI_TPW_POS_BOTTOM_LEFT,
  GUI_TPW_POS_TOP_CENTER,
  GUI_TPW_POS_TOP_RIGHT,
  GUI_TPW_POS_TOP_LEFT
} gui_TPW_Position;

gui_TPW* gui_createToplevelPopup(gui_Context* gui_context, int y, int x, int height, int width,
                                 gui_TPW_paintCallback paint_cb, gui_TPW_inputCallback input_cb,
                                 gui_TPW_destroyCallback destroy_cb, void* payload);

void gui_repaintTPW(gui_Context* gui_context);

void gui_updateTPW(gui_Context* gui_context);

// Complete API for gui_TPW
void gui_showTPW(gui_Context* gui_context, gui_TPW* popup);
void gui_setTPWVisibility(gui_Context* gui_context, gui_TPW* popup, bool visible);
void gui_setTPWStrongFocus(gui_Context* gui_context, gui_TPW* popup, bool strong_focus);
void gui_moveTPW(gui_Context* gui_context, gui_TPW* popup, int y, int x);
void gui_resizeTPW(gui_Context* gui_context, gui_TPW* popup, int height, int width);

// Predefined positioning helpers
void gui_calculateTPWPosition(gui_Context* gui_context, int height, int width, gui_TPW_Position pos, int* out_y,
                              int* out_x);
gui_TPW* gui_showTPWPositioned(gui_Context* gui_context, int height, int width, gui_TPW_Position position,
                               gui_TPW_paintCallback paint_cb, gui_TPW_inputCallback input_cb,
                               gui_TPW_destroyCallback destroy_cb, void* payload);

void gui_closeTPW(gui_Context* gui_context, gui_TPW* popup);
void gui_closeAllTPW(gui_Context* gui_context);
void gui_setTPWFocus(gui_Context* gui_context, gui_TPW* popup);

#endif // ALWIDE_TOPLEVEL_POPUP_H
