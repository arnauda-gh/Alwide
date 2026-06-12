#ifndef WISHWIM_EDITOR_CONTEXT_H
#define WISHWIM_EDITOR_CONTEXT_H

#include <stdbool.h>

#include "../data-management/file_management.h"
#include "../io-management/io_explorer.h"
#include "../terminal/highlight.h"
#include "../terminal/windows/edw.h"

typedef enum {
  EVENT_CONTINUE,
  EVENT_QUIT,
  EVENT_READ_INPUT,
} EventLoopAction;

typedef struct {
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
  time_val last_time_mouse_drag;
  time_val t_date;
  clock_t t_clock;
  bool quit_popup_active;
  bool force_quit;
} EditorContext;

FileContainer* getActiveFile(EditorContext* ctx);


#endif // WISHWIM_EDITOR_CONTEXT_H
