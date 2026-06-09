#include "graphics_tools.h"
#include <assert.h>
#include <limits.h>
#include <string.h>
#include "../data-management/encoding/utf8.h"
#include "../data-management/utf_8_extractor.h"

void countStringFrame(char* ch, int length, int* current_row, int* current_column, int* screen_max_width,
                      int tab_size) {
  assert(current_row != NULL);
  assert(current_column != NULL);

  const int line_length = (screen_max_width == NULL || *screen_max_width == 0) ? INT_MAX : *screen_max_width;

  int current_ch_index = 0;
  int current_line_length = 0;
  int max_line = 0;
  while (current_ch_index < length) {
    if (ch[current_ch_index] == '\n') {
      (*current_row)++;
      *current_column = 0;
      if (current_line_length > max_line) {
        max_line = current_line_length;
      }
      current_line_length = 0;
    }
    else {
      Char_U8 tmp_ch = readChar_U8FromCharArray(ch + current_ch_index);
      current_ch_index += sizeChar_U8(tmp_ch) - 1;
      (*current_column)++;
      if (current_line_length + charPrintSize(tmp_ch, tab_size) > line_length) {
        if (current_line_length > max_line) {
          max_line = current_line_length;
        }
        current_line_length = 0;
        (*current_row)++;
      }
      current_line_length += charPrintSize(tmp_ch, tab_size);
    }
    current_ch_index++;
  }
  if (current_line_length > max_line) {
    max_line = current_line_length;
  }

  if (screen_max_width != NULL) {
    *screen_max_width = max_line;
  }
}

void printToNcursesNCharFromString(WINDOW* w, char* str, int n) {
  for (int i = 0; i < n && str[i] != '\0'; i++) {
    wprintw(w, "%c", str[i]);
  }
}
