#include "theme.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "../utils/logger.h"


bool getThemeFromFile(char* file_name, HighlightThemeList* list) {
  initHighlightThemeList(list);

  FILE* f = fopen(file_name, "r");
  if (f == NULL) {
    notifyUser(NULL, LOG_WARNING, "Unable to open theme file %s.", file_name);
    return false;
  }


  int scan_res = 8;
  while (scan_res == 8) {
    HighlightTheme theme;
    scan_res =
      fscanf(f, " @%s ( %hd , %hd , %hd ) ( %hd , %hd , %hd ) \"%s\"", theme.group, &theme.color.r, &theme.color.g,
             &theme.color.b, &theme.color_hover.r, &theme.color_hover.g, &theme.color_hover.b, theme.attr);
    if (scan_res == 8) {
      addToHiglightThemeList(list, theme);
    }
  }

  fclose(f);
  return true;
}

void initHighlightThemeList(HighlightThemeList* list) {
  list->size = 0;
  list->groups = NULL;
}

void destroyThemeList(HighlightThemeList* list) { free(list->groups); }


void addToHiglightThemeList(HighlightThemeList* list, HighlightTheme theme) {
  list->size++;
  list->groups = realloc(list->groups, sizeof(HighlightTheme) * list->size);
  list->groups[list->size - 1] = theme;
}

attr_t getAttrForTheme(HighlightTheme theme) {
  attr_t attr = A_NORMAL;

  for (int i = 0; theme.attr[i] != '\0' && i < 10; i++) {
    switch (theme.attr[i]) {
      case 'i':
        attr |= A_ITALIC;
        break;
      case 'b':
        attr |= A_BOLD;
        break;
      case 'u':
        attr |= A_UNDERLINE;
        break;
      case 'r':
        attr |= A_REVERSE;
        break;
      case 'd':
        attr |= A_DIM;
        break;
      case 's':
        attr |= A_STANDOUT;
        break;
      case 'k':
        attr |= A_BLINK;
        break;
      case 'p':
        attr |= A_PROTECT;
        break;
      case 'v':
        attr |= A_INVIS;
        break;
      case '"':
        break;
      default:
        fprintf(stderr, "Attribute %c is not supported.\n", theme.attr[i]);
        exit(0);
    }
  }

  return attr;
}
