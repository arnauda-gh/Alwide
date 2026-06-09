#ifndef GRAPHICS_TOOLS_H
#define GRAPHICS_TOOLS_H

#include <ncurses.h>

void countStringFrame(char* ch, int length, int* current_row, int* current_column, int* screen_max_width, int tab_size);
void printToNcursesNCharFromString(WINDOW* w, char* str, int n);

#endif // GRAPHICS_TOOLS_H
