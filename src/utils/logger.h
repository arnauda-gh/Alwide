#ifndef ALWIDE_LOGGER_H
#define ALWIDE_LOGGER_H

#include "../core/editor_context.h"

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_CRITICAL
} LogLevel;

void notify_user(EditorContext* ctx, LogLevel level, const char* format, ...);
void set_notification_threshold(LogLevel level);

#endif // ALWIDE_LOGGER_H
