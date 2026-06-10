#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include "../terminal/windows/popups/notification_popup.h"

static LogLevel global_threshold = LOG_INFO;

static const char* level_to_str(LogLevel level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO: return "INFO";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR: return "ERROR";
        case LOG_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

void set_notification_threshold(LogLevel level) {
    global_threshold = level;
}

void notify_user(EditorContext* ctx, LogLevel level, const char* format, ...) {
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // 1. Log to stderr (logs.txt)
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_str[26];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(stderr, "[%s] %s - %s\n", level_to_str(level), time_str, message);
    fflush(stderr);

    // 2. Visual notification if above threshold
    if (level >= global_threshold && ctx != NULL) {
        gui_openNotificationPopup(ctx, message, level);
    }
}
