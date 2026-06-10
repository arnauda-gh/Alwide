#ifndef ALWIDE_NOTIFICATION_POPUP_H
#define ALWIDE_NOTIFICATION_POPUP_H

#include "../../windows/gui_entities.h"
#include "../../../core/editor_context.h"

void renderNotification(WINDOW* w, const char* message, int level, int width, int height);

typedef enum {
    L_DEBUG,
    L_INFO,
    L_WARNING,
    L_ERROR,
    L_CRITICAL
} NotifLogLevel;

void gui_openNotificationPopup(EditorContext* ctx, const char* message, int level);
void gui_updateNotificationsPosition(gui_Context* gui_context);
void gui_checkNotificationsExpiry(gui_Context* gui_context);

#endif // ALWIDE_NOTIFICATION_POPUP_H
