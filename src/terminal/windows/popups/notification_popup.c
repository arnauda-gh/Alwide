#include "notification_popup.h"
#include <stdlib.h>
#include <string.h>
#include "../../../environnement/constants.h"
#include "../../../utils/tools.h"
#include "../../graphics_tools.h"
#include "../../key_management.h"
#include "../tpw.h"

void renderNotification(WINDOW* w, const char* message, int level, int width, int height) {
  int color = INFO_COLOR_PAIR;
  char prefix[16] = "[INFO] ";
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

  werase(w);

  // Print prefix with color
  wattron(w, COLOR_PAIR(color) | A_BOLD);
  mvwprintw(w, 0, 1, "%s", prefix);

  // Print message using robust tool
  int prefix_len = strlen(prefix);
  printToWindow(w, (char*)message, strlen(message), prefix_len + 1, 0, width - prefix_len - 2, 1, 8);
  wattroff(w, COLOR_PAIR(color) | A_BOLD);
}

static void reposition_notifications(gui_Context* gui_context) {
  int current_index = 0;
  Notification* curr = gui_context->active_notifications;
  while (curr) {
    int width = curr->tpw->width;
    int height = curr->tpw->height;
    int y, x;
    gui_calculateTPWPosition(gui_context, height, width, GUI_TPW_POS_BOTTOM_RIGHT, &y, &x);
    y -= (current_index * 1);
    if (y < 0) {
      y = 0;
    }

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

  int width = strlen(n->message) + 10;
  if (width > 60) {
    width = 60;
  }
  if (width < 25) {
    width = 25;
  }
  int height = 1;

  int y, x;
  gui_calculateTPWPosition(&ctx->gui_context, height, width, GUI_TPW_POS_BOTTOM_RIGHT, &y, &x);
  y -= (n->index * 1);
  if (y < 0) {
    y = 0;
  }

  n->tpw = gui_createToplevelPopup(&ctx->gui_context, y, x, height, width, paint_notification_popup,
                                   input_notification_popup, destroy_notification_popup, n);
  if (n->tpw) {
    wbkgd(n->tpw->tpw, COLOR_PAIR(DEFAULT_COLOR_PAIR));
    n->tpw->expiry_time = timeInMilliseconds() + 3000;
    n->tpw->strong_focus = false;
  }
  else {
    // Cleanup if TPW creation failed
    // We pass ctx->gui_context if we want, but destroy_notification_popup gets it from n->gui_context
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
