#include "key_management.h"

#include <ctype.h>
#include <ncurses.h>
#include <stdio.h>
#include <string.h>

#include "../environnement/constants.h"
#include "../utils/tools.h"

time_val lastPress = 0;
int last_press_x = 0;
int last_press_y = 0;

time_val lastRelease = 0;
time_val lastClick = 0;
int last_clicked_x = 0;
int last_clicked_y = 0;

static void printIfPresent(MEVENT* event, unsigned long long value, char* print) {
  if (event->bstate & value) {
    fprintf(stderr, "%s, ", print);
  }
}

void printEventList(MEVENT* event) {
  printIfPresent(event, BUTTON1_PRESSED, "BUTTON1_PRESSED");
  printIfPresent(event, BUTTON1_RELEASED, "BUTTON1_RELEASED");
  printIfPresent(event, BUTTON1_CLICKED, "BUTTON1_CLICKED");
  printIfPresent(event, BUTTON1_DOUBLE_CLICKED, "BUTTON1_DOUBLE_CLICKED");


  printIfPresent(event, BUTTON2_PRESSED, "BUTTON2_PRESSED");
  printIfPresent(event, BUTTON2_RELEASED, "BUTTON2_RELEASED");
  printIfPresent(event, BUTTON2_CLICKED, "BUTTON2_CLICKED");
  printIfPresent(event, BUTTON2_DOUBLE_CLICKED, "BUTTON2_DOUBLE_CLICKED");


  printIfPresent(event, BUTTON3_PRESSED, "BUTTON3_PRESSED");
  printIfPresent(event, BUTTON3_RELEASED, "BUTTON3_RELEASED");
  printIfPresent(event, BUTTON3_CLICKED, "BUTTON3_CLICKED");

  printIfPresent(event, BUTTON4_PRESSED, "BUTTON4_PRESSED");
  printIfPresent(event, BUTTON4_RELEASED, "BUTTON4_RELEASED");
  printIfPresent(event, BUTTON4_CLICKED, "BUTTON4_CLICKED");

  // BUTTON5_* is not available on all ncurses implementations, e.g. macOS.
  #ifdef BUTTON5_PRESSED
    printIfPresent(event, BUTTON5_PRESSED, "BUTTON5_PRESSED");
  #endif
  #ifdef BUTTON5_RELEASED
    printIfPresent(event, BUTTON5_RELEASED, "BUTTON5_RELEASED");
  #endif
  #ifdef BUTTON5_CLICKED
    printIfPresent(event, BUTTON5_CLICKED, "BUTTON5_CLICKED");
  #endif

  printIfPresent(event, BUTTON6_PRESSED, "BUTTON6_PRESSED");
  printIfPresent(event, BUTTON7_PRESSED, "BUTTON7_PRESSED");
  printIfPresent(event, BUTTON8_PRESSED, "BUTTON8_PRESSED");
  printIfPresent(event, BUTTON9_PRESSED, "BUTTON9_PRESSED");
}


// Hard to implement bc there are different behaviour depend on mouse or touchpad is used.
void detectComplexMouseEvents(MEVENT* event) {
  printEventList(event);
  if (event->bstate != NO_EVENT_MOUSE) {
    fprintf(stderr, "\n");
  }
  time_val current_time = timeInMilliseconds();

  // fprintf(stderr, "BEGIN > id: %d, pos: (%d, %d, %d), masks: ", event->id, event->x, event->y, event->z);
  // printEventList(event);
  // fprintf(stderr, "\n    ==>");

  event->bstate = event->bstate & ~BUTTON1_CLICKED;


  if (event->bstate & BUTTON1_PRESSED && event->bstate & BUTTON1_RELEASED) {
    event->bstate = event->bstate & ~BUTTON1_PRESSED;
    event->bstate = event->bstate & ~BUTTON1_RELEASED;
    event->bstate |= BUTTON1_CLICKED;
  }
  else if (event->bstate & BUTTON1_RELEASED || event->bstate & BUTTON1_PRESSED) {
    if (diff2Time(current_time, lastPress) < TIME_BETWEEN_EVENT && event->x == last_press_x &&
        event->y == last_press_y) {
      if (event->bstate & BUTTON1_RELEASED && diff2Time(current_time, lastClick) < 2 * TIME_BETWEEN_EVENT &&
          event->x == last_clicked_x && event->y == last_clicked_y) {
        event->bstate |= BUTTON1_DOUBLE_CLICKED;
        event->bstate = event->bstate & ~BUTTON1_CLICKED;
      }
      else {
        event->bstate |= BUTTON1_CLICKED;
        event->bstate = event->bstate & ~BUTTON1_PRESSED;
        event->bstate = event->bstate | BUTTON1_RELEASED;
      }
    }
  }

  if (event->bstate & BUTTON1_PRESSED) {
    // printf("Pressed\n\r");
    lastPress = current_time;
    last_press_x = event->x;
    last_press_y = event->y;
  }
  if (event->bstate & BUTTON1_RELEASED) {
    // printf("Released\r\n");
    lastRelease = current_time;
  }
  if (event->bstate & BUTTON1_CLICKED) {
    // printf("Detected click !\n\r");
    lastClick = current_time;
    last_clicked_x = event->x;
    last_clicked_y = event->y;

    lastPress = current_time;
    last_press_x = event->x;
    last_press_y = event->y;
  }
  if (event->bstate & BUTTON1_DOUBLE_CLICKED) {
    // printf("Detected double click !\n\r");
    // lastClick = 0;
  }

  if (event->bstate & BUTTON4_PRESSED) {
    MEVENT tmp_event;
    tmp_event.bstate = BUTTON4_RELEASED;
    tmp_event.x = event->x;
    tmp_event.y = event->y;
    tmp_event.z = event->z;
  }

  #ifdef BUTTON5_PRESSED
    if (event->bstate & BUTTON5_PRESSED) {
      MEVENT tmp_event;
      tmp_event.bstate = BUTTON5_RELEASED;
      tmp_event.x = event->x;
      tmp_event.y = event->y;
      tmp_event.z = event->z;
    }
  #endif
}

int normalize_legacy(int c) {
  if (c == ERR) {
    return ERR;
  }

  /* 1. Handle Ncurses-translated shifted/modified keys */
  switch (c) {
    case KEY_BTAB:
      return K_SPECIAL(K_MOD_SHIFT, KEY_BTAB);
    case KEY_SRIGHT:
      return K_SPECIAL(K_MOD_SHIFT, KEY_RIGHT);
    case KEY_SLEFT:
      return K_SPECIAL(K_MOD_SHIFT, KEY_LEFT);
    case KEY_SHOME:
      return K_SPECIAL(K_MOD_SHIFT, KEY_HOME);
    case KEY_SEND:
      return K_SPECIAL(K_MOD_SHIFT, KEY_END);
    case KEY_SDC:
      return K_SPECIAL(K_MOD_SHIFT, KEY_DC);
    case KEY_SIC:
      return K_SPECIAL(K_MOD_SHIFT, KEY_IC);
    case KEY_SNEXT:
      return K_SPECIAL(K_MOD_SHIFT, KEY_NPAGE);
    case KEY_SR:
      return K_SPECIAL(K_MOD_SHIFT, KEY_UP);
    case KEY_SF:
      return K_SPECIAL(K_MOD_SHIFT, KEY_DOWN);
  }

  /* 2. Map ASCII control codes 1-31 to Unified format. */
  if (((c >= 1 && c <= 31) || c == 0) && c != 9 && c != 10 && c != 13) {
    int base;
    if (c == 0) {
      base = ' ';
    }
    else if (c >= 1 && c <= 26) {
      base = c + 'a' - 1;
    }
    else if (c == 27) {
      return H_KEY_ESCAPE;
    }
    else if (c == 28) {
      base = '\\';
    }
    else if (c == 29) {
      base = ']';
    }
    else if (c == 30) {
      base = '^';
    }
    else if (c == 31) {
      base = '_';
    }
    else {
      base = c;
    }
    return K_SPECIAL(K_MOD_CTRL, base);
  }

  /* 3. Standard keycodes */
  switch (c) {
    case 9:
      return H_KEY_TAB;
    case 10:
    case 13:
      return H_KEY_ENTER;
    case 127:
      return K_SPECIAL(0, KEY_BACKSPACE);
    default:
      if (c > 255) {
        /* Ultimate fallback: parse dynamically via Ncurses terminfo keyname */
        const char* name = keyname(c);
        if (name && name[0] == 'k') {
          int target_key = 0;
          if (strstr(name, "kUP") || strstr(name, "kcuu")) {
            target_key = KEY_UP;
          }
          else if (strstr(name, "kDN") || strstr(name, "kcud")) {
            target_key = KEY_DOWN;
          }
          else if (strstr(name, "kLFT") || strstr(name, "kcub")) {
            target_key = KEY_LEFT;
          }
          else if (strstr(name, "kRGT") || strstr(name, "kRIT") || strstr(name, "kcuf")) {
            target_key = KEY_RIGHT;
          }
          else if (strstr(name, "kHOM") || strstr(name, "khome")) {
            target_key = KEY_HOME;
          }
          else if (strstr(name, "kEND")) {
            target_key = KEY_END;
          }
          else if (strstr(name, "kDC")) {
            target_key = KEY_DC;
          }
          else if (strstr(name, "kIC")) {
            target_key = KEY_IC;
          }
          else if (strstr(name, "kPRV") || strstr(name, "kpp")) {
            target_key = KEY_PPAGE;
          }
          else if (strstr(name, "kNXT") || strstr(name, "knp")) {
            target_key = KEY_NPAGE;
          }

          if (target_key != 0) {
            int len = strlen(name);
            /* Search for a digit at the end of the name (e.g., kUP5) */
            for (int i = len - 1; i >= 0 && isdigit((unsigned char)name[i]); i--) {
              int val = (name[i] - '0') - 1;
              if (val < 0) {
                continue;
              }

              int unified_mods = 0;
              if (val & 1) {
                unified_mods |= K_MOD_SHIFT;
              }
              if (val & 2) {
                unified_mods |= K_MOD_ALT;
              }
              if (val & 4) {
                unified_mods |= K_MOD_CTRL;
              }
              if (val & 8) {
                unified_mods |= K_MOD_SUPER;
              }
              return K_SPECIAL(unified_mods, target_key);
            }
            return K_SPECIAL(0, target_key);
          }
        }
        return K_SPECIAL(0, c); /* Other Ncurses keys */
      }
      return c; /* Standard printable byte */
  }
}

void logInput(int key) {
  if (key == ERR) {
    return;
  }

  FILE* f = stderr;

  fprintf(stderr, "[INPUT] Key: 0x%08X | ", key);

  /* Modifiers */
  if (K_HAS_CTRL(key)) {
    fprintf(f, "CTRL+");
  }
  if (K_HAS_ALT(key)) {
    fprintf(f, "ALT+");
  }
  if (K_HAS_SHIFT(key)) {
    fprintf(f, "SHIFT+");
  }
  if (K_HAS_SUPER(key)) {
    fprintf(f, "SUPER+");
  }

  int codepoint = K_CODE(key);

  if (K_IS_SPECIAL(key)) {
    fprintf(f, "SPECIAL: code : %d ", codepoint);
    if (codepoint >= KEY_MIN && codepoint <= KEY_MAX) {
      fprintf(f, ": %s", keyname(codepoint));
    }
    else if (codepoint == 27) {
      fprintf(f, ": ESCAPE");
    }

    if (codepoint >= 32 && codepoint <= 126) {
      fprintf(f, " : char : %c", (char)codepoint);
    }
  }
  else {
    Char_U8 u8 = unicode_to_utf8(codepoint);
    fprintf(f, "CHAR: '");
    for (int i = 0; i < 4 && u8.t[i]; i++) {
      fprintf(f, "%c", u8.t[i]);
    }
    fprintf(f, "' (U+%04X)", codepoint);
  }

  if (K_IS_RELEASE(key)) {
    fprintf(f, " [RELEASE]");
  }
  else {
    fprintf(f, " [PRESS]");
  }

  fprintf(f, "\n");
}
