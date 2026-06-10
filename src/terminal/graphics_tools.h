#ifndef GRAPHICS_TOOLS_H
#define GRAPHICS_TOOLS_H

#include <ncurses.h>

#include "../data-management/utf_8_extractor.h"

/**
 * Calculates the number of rows and columns a string will occupy on screen.
 * Handles UTF-8 characters and tab expansion.
 */
void countStringFrame(char* ch, int length, int* current_row, int* current_column, int* screen_max_width, int tab_size);

/**
 * Prints up to n characters from a string to an ncurses window.
 */
void printToNcursesNCharFromString(WINDOW* w, char* str, int n);

/**
 * Robustly prints a string to an ncurses window with wrapping and scrolling support.
 * @param w The ncurses window
 * @param ch The string to print
 * @param length Length of the string
 * @param offset_x Starting X position
 * @param offset_y Starting Y position
 * @param line_length Maximum width of a line before wrapping
 * @param max_line_number Maximum number of lines to print (0 for unlimited)
 * @param tab_size Number of spaces per tab
 */
void printToWindow(WINDOW* w, char* ch, int length, int offset_x, int offset_y, int line_length, int max_line_number,
                   int tab_size);

/**
 * Prints a single UTF-8 character to an ncurses window.
 */
void gui_printChar_U8ToNcurses(WINDOW* w, Char_U8 ch);

#endif // GRAPHICS_TOOLS_H
