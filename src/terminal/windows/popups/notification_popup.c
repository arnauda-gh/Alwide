#include "notification_popup.h"
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include "../../../environnement/constants.h"
#include "../../../utils/tools.h"
#include "../../../data-management/utf_8_extractor.h"
#include "../../graphics_tools.h"
#include "../../key_management.h"
#include "../tpw.h"

static int calculate_message_height(const char* message, int line_length, int tab_size) {
  if (!message || line_length <= 0) {
    return 0;
  }

  int length = strlen(message);
  int current_row = 0;
  int current_ch_index = 0;
  int current_line_length = 0;

  while (current_ch_index < length) {
    if (message[current_ch_index] == '\n') {
      current_line_length = 0;
      current_row++;
    }
    else {
      Char_U8 tmp_ch = readChar_U8FromCharArray((char*)message + current_ch_index);
      current_ch_index += sizeChar_U8(tmp_ch) - 1;

      int char_size = charPrintSize(tmp_ch, tab_size);
      if (current_line_length + char_size > line_length) {
        current_line_length = 0;
        current_row++;
      }
      current_line_length += char_size;
    }
    current_ch_index++;
  }
  return current_row + 1;
}

void renderNotification(WINDOW* w, const char* message, int level, int width, int height) {
  int color = INFO_COLOR_PAIR;
  char prefix[16] = "[DEBUG] ";
  if (level >= 4) { // CRITICAL
    color = ERROR_COLOR_PAIR;
    strcpy(prefix, "[CRIT] ");
  }
  else if (level == 3) { // ERROR
    color = ERROR_COLOR_PAIR;
    strcpy(prefix, "[ERR ] ");
  }
  else if (level == 2) { // WARNING
    color = WARNING_COLOR_PAIR;
    strcpy(prefix, "[WARN] ");
  }
  else if (level == 1) { // INFO
    color = INFO_COLOR_PAIR;
    strcpy(prefix, "[INFO] ");
  }

  werase(w);

  // Print prefix with color
  wattron(w, COLOR_PAIR(color) | A_BOLD);
  mvwprintw(w, 0, 1, "%s", prefix);

  // Print message using robust tool
  int prefix_len = strlen(prefix);
  printToWindow(w, (char*)message, strlen(message), prefix_len + 1, 0, width - prefix_len - 2, height, 8);
  wattroff(w, COLOR_PAIR(color) | A_BOLD);
}

static void reposition_notifications(gui_Context* gui_context) {
  int current_index = 0;
  Notification* curr = gui_context->active_notifications;
  int last_y = -1;
  while (curr) {
    int width = curr->tpw->width;
    int height = curr->tpw->height;
    int y, x;
    gui_calculateTPWPosition(gui_context, height, width, GUI_TPW_POS_BOTTOM_RIGHT, &y, &x);
    if (current_index > 0) {
      y = last_y - height;
    }
    if (y < 0) {
      y = 0;
    }
    last_y = y;

    gui_moveTPW(gui_context, curr->tpw, y, x);
    curr->index = current_index;

    current_index++;
    curr = curr->next;
  }
}

static void paint_notification_popup(gui_TPW* popup, void* payload) {
  Notification* n = (Notification*)payload;
  renderNotification(popup->tpw, n->message, n->level, popup->width, popup->height);
}

static bool input_notification_popup(gui_TPW* popup, int key, MEVENT* m_event, void* payload) { return false; }

static void destroy_notification_popup(gui_TPW* popup, void* payload) {
  Notification* n = (Notification*)payload;
  gui_Context* gui_context = n->gui_context;

  // Remove from active_notifications list
  Notification* prev = NULL;
  Notification* curr = gui_context->active_notifications;
  while (curr && curr != n) {
    prev = curr;
    curr = curr->next;
  }

  if (curr) {
    if (prev) {
      prev->next = curr->next;
    }
    else {
      gui_context->active_notifications = curr->next;
    }

    gui_context->notification_count--;
    reposition_notifications(gui_context);
  }

  free(n);
}

void gui_openNotificationPopup(EditorContext* ctx, const char* message, int level) {
  Notification* n = malloc(sizeof(Notification));
  if (!n) {
    return;
  }

  strncpy(n->message, message, sizeof(n->message) - 1);
  n->message[sizeof(n->message) - 1] = '\0';
  n->level = level;
  n->index = ctx->gui_context.notification_count;
  n->gui_context = &ctx->gui_context;
  n->next = NULL;

  // Append to active_notifications
  if (!ctx->gui_context.active_notifications) {
    ctx->gui_context.active_notifications = n;
  }
  else {
    Notification* curr = ctx->gui_context.active_notifications;
    while (curr->next) {
      curr = curr->next;
    }
    curr->next = n;
  }
  ctx->gui_context.notification_count++;

  int max_width = 60;
  if (max_width > COLS - 4) {
    max_width = COLS - 4;
  }
  if (max_width < 25) {
    max_width = 25;
  }

  int prefix_len = 7; // "[INFO] ", "[WARN] ", etc.
  int width = strlen(n->message) + prefix_len + 3;
  int height = 1;

  if (width > max_width) {
    width = max_width;
    int line_length = width - prefix_len - 2;
    height = calculate_message_height(n->message, line_length, 8);
  }

  if (height > 10) {
    height = 10;
  }

  int y, x;
  gui_calculateTPWPosition(&ctx->gui_context, height, width, GUI_TPW_POS_BOTTOM_RIGHT, &y, &x);
  if (n->index > 0) {
    // We stack them relative to the previous notifications
    // reposition_notifications will handle this correctly for all notifications,
    // but we need a starting y for this one before reposition_notifications or gui_createToplevelPopup.
    // So we run reposition_notifications at the end of creation, but let's give it a reasonable starting position.
  }

  n->tpw = gui_createToplevelPopup(&ctx->gui_context, y, x, height, width, paint_notification_popup,
                                   input_notification_popup, destroy_notification_popup, n);
  if (n->tpw) {
    wbkgd(n->tpw->tpw, COLOR_PAIR(DEFAULT_COLOR_PAIR));
    
    int duration_ms = 3000;
    switch (level) {
      case 0: // LOG_DEBUG
      case 1: // LOG_INFO
        duration_ms = 2000;
        break;
      case 2: // LOG_WARNING
        duration_ms = 5000;
        break;
      case 3: // LOG_ERROR
        duration_ms = 8000;
        break;
      case 4: // LOG_CRITICAL
        duration_ms = 12000;
        break;
    }
    
    n->tpw->expiry_time = timeInMilliseconds() + duration_ms;
    n->tpw->strong_focus = false;
    // Reposition all notifications to align correctly
    reposition_notifications(&ctx->gui_context);
  }
  else {
    // Cleanup if TPW creation failed
    destroy_notification_popup(NULL, n);
  }
}

// Public function to trigger repositioning if needed
void gui_updateNotificationsPosition(gui_Context* gui_context) { reposition_notifications(gui_context); }

void gui_checkNotificationsExpiry(gui_Context* gui_context) {
  // Handle auto-destruction of temporary popups
  gui_TPW* curr = gui_context->toplevel_popups;
  long long now = timeInMilliseconds();
  while (curr) {
    gui_TPW* next = curr->next;
    if (curr->expiry_time > 0 && now > curr->expiry_time) {
      gui_destroyToplevelPopup(gui_context, curr);
    }
    curr = next;
  }
}
