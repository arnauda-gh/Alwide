#include "tpw.h"
#include <stdlib.h>
#include "../../environnement/constants.h"
#include "../term_handler.h"

gui_TPW* gui_createToplevelPopup(gui_Context* gui_context, int y, int x, int height, int width,
                                 gui_TPW_paintCallback paint_cb, gui_TPW_inputCallback input_cb,
                                 gui_TPW_destroyCallback destroy_cb, void* payload) {

  gui_TPW* tpw_context = malloc(sizeof(gui_TPW));
  if (!tpw_context) {
    return NULL;
  }

  tpw_context->tpw = newwin(height, width, y, x);
  tpw_context->y = y;
  tpw_context->x = x;
  tpw_context->height = height;
  tpw_context->width = width;
  tpw_context->visible = true;
  tpw_context->on_paint = paint_cb;
  tpw_context->on_input = input_cb;
  tpw_context->on_destroy = destroy_cb;
  tpw_context->payload = payload;
  tpw_context->expiry_time = 0;
  tpw_context->next = NULL;
  tpw_context->strong_focus = true;

  // Prepend to the linked list
  gui_addNewTPW(gui_context, tpw_context);

  // Set focus on creation
  gui_updateTPW(gui_context);


  // Style the window
  if (tpw_context->tpw) {
    wbkgd(tpw_context->tpw, COLOR_PAIR(INFO_COLOR_PAIR));
    keypad(tpw_context->tpw, TRUE);
  }

  return tpw_context;
}

static void gui_repaintTPW_recursive(gui_TPW* popup) {
  if (popup == NULL) {
    return;
  }
  // Paint the rest of the list first (older popups)
  gui_repaintTPW_recursive(popup->next);

  // Paint this popup (newer popup)
  if (popup->visible && popup->tpw != NULL) {
    if (popup->on_paint) {
      popup->on_paint(popup, popup->payload);
    }
    wnoutrefresh(popup->tpw);
  }
}

void gui_repaintTPW(gui_Context* gui_context) { gui_repaintTPW_recursive(gui_context->toplevel_popups); }

void gui_updateTPW(gui_Context* gui_context) { gui_context->refresh_tpw = true; }

void gui_destroyToplevelPopup(gui_Context* gui_context, gui_TPW* popup) {
  if (!popup) {
    return;
  }

  // Remove from linked list
  gui_TPW* prev = NULL;
  gui_TPW* curr = gui_context->toplevel_popups;
  while (curr != NULL && curr != popup) {
    prev = curr;
    curr = curr->next;
  }

  if (curr == popup) {
    if (prev == NULL) {
      gui_context->toplevel_popups = popup->next;
    }
    else {
      prev->next = popup->next;
    }
  }

  // Call destroy callback
  if (popup->on_destroy) {
    popup->on_destroy(popup, popup->payload);
  }

  // Destroy ncurses window
  if (popup->tpw) {
    delwin(popup->tpw);
  }

  free(popup);

  gui_updateGUI(gui_context);
}

void gui_destroyAllToplevelPopups(gui_Context* gui_context) {
  while (gui_context->toplevel_popups != NULL) {
    gui_destroyToplevelPopup(gui_context, gui_context->toplevel_popups);
  }
}

void gui_setToplevelPopupFocus(gui_Context* gui_context, gui_TPW* popup) {
  if (!popup) {
    gui_updateTPW(gui_context);
    return;
  }

  // Raise popup to the head of the list so it is painted on top
  if (gui_context->toplevel_popups != popup) {
    gui_TPW* prev = NULL;
    gui_TPW* curr = gui_context->toplevel_popups;
    while (curr != NULL && curr != popup) {
      prev = curr;
      curr = curr->next;
    }
    if (curr == popup) {
      if (prev != NULL) {
        prev->next = popup->next;
      }
      popup->next = gui_context->toplevel_popups;
      gui_context->toplevel_popups = popup;
    }
  }
  gui_updateTPW(gui_context);
}

void gui_addNewTPW(gui_Context* gui_context, gui_TPW* popup) {
  if (!popup) {
    return;
  }
  popup->next = gui_context->toplevel_popups;
  gui_context->toplevel_popups = popup;
}

void gui_setTPWVisibility(gui_Context* gui_context, gui_TPW* popup, bool visible) {
  if (!popup) {
    return;
  }
  popup->visible = visible;
  gui_updateTPW(gui_context);
}

void gui_setTPWFocus(gui_Context* gui_context, gui_TPW* popup, bool has_focus) {
  if (!popup) {
    return;
  }
  if (has_focus) {
    gui_setToplevelPopupFocus(gui_context, popup);
  }
}

void gui_setTPWStrongFocus(gui_Context* gui_context, gui_TPW* popup, bool strong_focus) {
  if (!popup) {
    return;
  }
  popup->strong_focus = strong_focus;
  gui_updateTPW(gui_context);
}

void gui_moveTPW(gui_Context* gui_context, gui_TPW* popup, int y, int x) {
  if (!popup) {
    return;
  }
  popup->y = y;
  popup->x = x;
  if (popup->tpw) {
    mvwin(popup->tpw, y, x);
  }
  gui_updateTPW(gui_context);
}

void gui_resizeTPW(gui_Context* gui_context, gui_TPW* popup, int height, int width) {
  if (!popup) {
    return;
  }
  popup->height = height;
  popup->width = width;
  if (popup->tpw) {
    wresize(popup->tpw, height, width);
  }
  gui_updateTPW(gui_context);
}

void gui_calculateTPWPosition(gui_Context* gui_context, int height, int width, gui_TPW_Position pos, int* out_y,
                              int* out_x) {
  int screen_h = LINES;
  int screen_w = COLS;

  int y = 0;
  int x = 0;

  switch (pos) {
    case GUI_TPW_POS_CENTER:
      y = (screen_h - height) / 2;
      x = (screen_w - width) / 2;
      break;
    case GUI_TPW_POS_BOTTOM:
      y = screen_h - height - 1;
      x = (screen_w - width) / 2;
      break;
    case GUI_TPW_POS_BOTTOM_RIGHT:
      y = screen_h - height - 1;
      x = screen_w - width - 2;
      break;
    case GUI_TPW_POS_BOTTOM_LEFT:
      y = screen_h - height - 1;
      x = 2;
      break;
    case GUI_TPW_POS_TOP_CENTER:
      y = 1;
      x = (screen_w - width) / 2;
      break;
    case GUI_TPW_POS_TOP_RIGHT:
      y = 1;
      x = screen_w - width - 2;
      break;
    case GUI_TPW_POS_TOP_LEFT:
      y = 1;
      x = 2;
      break;
    case GUI_TPW_POS_CUSTOM:
    default:
      break;
  }

  if (y + height > screen_h) {
    y = screen_h - height;
  }
  if (x + width > screen_w) {
    x = screen_w - width;
  }

  if (y < 0) {
    y = 0;
  }
  if (x < 0) {
    x = 0;
  }

  if (out_y) {
    *out_y = y;
  }
  if (out_x) {
    *out_x = x;
  }
}

gui_TPW* gui_createToplevelPopupPositioned(gui_Context* gui_context, int height, int width, gui_TPW_Position position,
                                           gui_TPW_paintCallback paint_cb, gui_TPW_inputCallback input_cb,
                                           gui_TPW_destroyCallback destroy_cb, void* payload) {
  int y = 0, x = 0;
  gui_calculateTPWPosition(gui_context, height, width, position, &y, &x);
  return gui_createToplevelPopup(gui_context, y, x, height, width, paint_cb, input_cb, destroy_cb, payload);
}
