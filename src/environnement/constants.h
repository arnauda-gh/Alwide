#ifndef CONSTANTS_H
#define CONSTANTS_H

#define LANG_ID_LENGTH 100

// #define PARSE_PRINT
#define SIMULATED_CURSOR

#define SCROLL_SPEED   2
#define NO_EVENT_MOUSE 268435456

#define FILE_NAME_SEPARATOR       " | "
#define OPENED_FILE_WINDOW_HEIGHT 2 // 2 to enable 0 to disable

#define FILE_EXPLORER_WIDTH       30
#define FILE_EXPLORER_TREE_OFFSET 1

#define BG_COLOR_DEFAULT 8
#define BG_COLOR_HOVER   9
#define BG_COLOR_POPUP   10
#define COLOR_TRUE_WHITE 11

#define COLOR_HOVER_OFFSET       1000
#define DEFAULT_COLOR_PAIR       1
#define DEFAULT_COLOR_HOVER_PAIR 1001
#define ERROR_COLOR_PAIR         2
#define ERROR_COLOR_HOVER_PAIR   1002

#define WARNING_COLOR_PAIR       3
#define WARNING_COLOR_HOVER_PAIR 1003

#define INFO_COLOR_PAIR       4
#define INFO_COLOR_HOVER_PAIR 1004

#define STATUS_BAR_COLOR_PAIR 5

#define STATUS_BAR_BUTTON_TEXT " < "

#define MAX_SIZE_FILE_LOGIC 4000000 /*4 Mb*/

#define SKIP_MOUSE_EVENT_DELAY 0

extern int color_index;
extern int color_pair;

#endif
