#ifndef TEXT_BOX_H
#define TEXT_BOX_H

#include <ncurses.h>
#include "../../../data-management/file_management.h"

void renderTextBuffer(WINDOW* w, TextBuffer* tb, int offset_x, int offset_y, int line_length, int max_line_number);

#endif // TEXT_BOX_H
