#ifndef WISHWIM_GUI_ENTITIES_H
#define WISHWIM_GUI_ENTITIES_H
#include <ncurses.h>

#include "../../data-management/file_structure.h"

typedef struct gui_TPW gui_TPW;

typedef void (*gui_TPW_paintCallback)(gui_TPW* popup, void* payload);
typedef bool (*gui_TPW_inputCallback)(gui_TPW* popup, int key, MEVENT* m_event, void* payload);
typedef void (*gui_TPW_destroyCallback)(gui_TPW* popup, void* payload);

struct gui_TPW {
  WINDOW* tpw;       // The NCurses window pointer
  int y;             // Screen y position (top-left)
  int x;             // Screen x position (top-left)
  int height;        // Window height
  int width;         // Window width
  bool visible;      // Is the popup currently displayed
  bool strong_focus; // Does the popup need to be closed to allow lower level to get input.

  gui_TPW_paintCallback on_paint;     // Paint callback
  gui_TPW_inputCallback on_input;     // Input handler callback
  gui_TPW_destroyCallback on_destroy; // Cleanup callback

  void* payload; // Custom data specific to this popup instance

  long long expiry_time; // Time in ms when the popup should be destroyed (0 for permanent)

  gui_TPW* next; // Next popup in the linked list
};


typedef struct {
  // NCurses items
  WINDOW* ofw; // Opened Files Window

  // Local data
  bool refresh_ofw; // Need to reprint opened file window

  int current_file_offset;
  int ofw_height; // Height of Opened Files Window. 0 => Disabled on start.   OPENED_FILE_WINDOW_HEIGHT => Enabled on
  // start.
} gui_OFW;

typedef struct {
  // NCurses items
  WINDOW* few; // File Explorer Window

  // Local data
  bool refresh_few; // Need to reprint file explorer window

  int few_width; // File explorer width
  int saved_few_width;
  int few_x_offset; /* TODO unused */
  int few_y_offset; // Y Scroll state of File Explorer Window
  int few_selected_line;
} gui_FEW;


typedef enum { DIAGNOSTICS, COMPLETION, HOVER_DIAGNOSTICS, GOTO_CHOICE, SIGNATURE_HELP, NO_OWNER } PopupOwner;

typedef struct {
  // NCurses items
  WINDOW* ftw; // File Text Window
  WINDOW* lnw; // Line Number Window
  WINDOW* pow; // Popup Window
  WINDOW* sbw; // Status Bar Window

  // Local data
  bool refresh_edw; // Need to reprint editor window
  bool show_sbw;    // Status Bar Toggle
  bool sbw_hovered; // Status Bar Hover state

  // lnw vars
  int length_line_number;

  // Popup vars
  bool show_pow;
  PopupOwner pow_owner;
  int item_selected;
  int item_select_offset_y;
  CursorDescriptor lastTextAnchor;
  CursorDescriptor lastMousePosition;
} gui_EDW;


typedef struct gui_Context gui_Context;
typedef struct Notification Notification;

struct Notification {
  gui_TPW* tpw;
  char message[256];
  int level;
  int index; // Position in stack
  gui_Context* gui_context;
  Notification* next;
};

struct gui_Context {
  // Init GUI vars
  gui_EDW edw_context; // Editor Window Context
  gui_OFW ofw_context; // Opened File Context
  gui_FEW few_context; // File Explorer Context

  // Used to set the window where start mouse drag
  WINDOW* focus_w;

  // Toplevel popup list head
  gui_TPW* toplevel_popups;
  bool refresh_tpw;

  // Notifications
  Notification* active_notifications;
  int notification_count;
};


#endif // WISHWIM_GUI_ENTITIES_H
