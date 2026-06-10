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

void notifyUser(EditorContext* ctx, LogLevel level, const char* format, ...);
void setNotificationThreshold(LogLevel level);
void setActiveContext(EditorContext* ctx);

#endif // ALWIDE_LOGGER_H
