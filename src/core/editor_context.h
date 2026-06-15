#ifndef WISHWIM_EDITOR_CONTEXT_H
#define WISHWIM_EDITOR_CONTEXT_H

#include <stdbool.h>

#include "../data-management/file_management.h"
#include "../io-management/io_explorer.h"
#include "../terminal/highlight.h"
#include "../terminal/windows/edw.h"
#include "features/navigation_history.h"

typedef enum {
  EVENT_CONTINUE,
  EVENT_QUIT,
  EVENT_READ_INPUT,
} EventLoopAction;

typedef struct EditorContext {
  FileContainer* files;
  int file_count;
  int current_file_index;
  ExplorerFolder pwd;
  gui_Context gui_context;
  bool refresh_local_vars;

  WindowHighlightDescriptor highlight_descriptor;
  History* old_history_frame;
  PayloadStateChange payload_state_change;
  Cursor old_selected_cursor;

  MEVENT m_event;
  int peek_c;
  bool mouse_drag;

  bool quit_popup_active;
  bool force_quit;

  NavigationHistory nav_history;
} EditorContext;

FileContainer* getActiveFile(EditorContext* ctx);


#endif // WISHWIM_EDITOR_CONTEXT_H
