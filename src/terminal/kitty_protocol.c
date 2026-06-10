#include "kitty_protocol.h"
#include <ctype.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include "key_management.h"

void kitty_enable(void) {
  /* Enable flag 1 (disambiguate) and flag 2 (event types/releases) */
  printf("\033[=1u");
  fflush(stdout);
}

void kitty_disable(void) {
  /* Reset keyboard mode */
  printf("\033[0u");
  fflush(stdout);
}

bool kitty_parse_sequence(int first_char, KittyKeyEvent* event, MEVENT* mouse_event, int* out_unread) {
  *out_unread = ERR;
  if (first_char != 27) {
    return false;
  }

  /* Patience: Wait up to 100ms for the very first char after Escape */
  int next = getch();

  if (next == ERR) {
    event->key_code = 27;
    event->modifiers = 1;
    event->type = KITTY_EVENT_PRESS;
    return true;
  }

  if (next != '[' && next != 'O') {
    /* Legacy Fallback: ESC + Enter/Newline is often Shift+Enter or Alt+Enter */
    if (next == '\r' || next == '\n') {
      event->key_code = 13;
      event->modifiers = 2; /* Map to Shift bitmask */
      event->type = KITTY_EVENT_PRESS;
      return true;
    }
    *out_unread = next;
    event->key_code = 27;
    event->modifiers = 1;
    event->type = KITTY_EVENT_PRESS;
    return true;
  }

  /* We are inside a sequence (CSI or SS3). Consume until terminator. */
  char buf[64];
  int len = 0;
  char terminator = '\0';

  /* For the body of the sequence, wait up to 100ms per byte */
  while (len < (int)sizeof(buf) - 1) {
    int c = getch();
    if (c == ERR) {
      break; /* Sequence ended prematurely */
    }

    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '~') {
      terminator = (char)c;
      break;
    }
    buf[len++] = (char)c;
  }
  buf[len] = '\0';

  if (terminator == '\0') {
    /* Premature end: swallow what we have to avoid garbage */
    event->key_code = -2;
    event->modifiers = 1;
    event->type = KITTY_EVENT_PRESS;
    return true;
  }

  /* 1.5 Parse SGR mouse sequence */
  if (buf[0] == '<') {
    int button = 0, x = 0, y = 0;
    if (sscanf(buf + 1, "%d;%d;%d", &button, &x, &y) == 3) {
      mouse_event->x = x - 1;
      mouse_event->y = y - 1;
      mouse_event->z = 0;
      mouse_event->id = 0;
      mouse_event->bstate = 0;

      int base_button = button;
      if (button >= 64) {
        base_button = (button & 3) + 64;
      }
      else {
        base_button = button & (3 | 32);
      }

      if (button & 4) {
        mouse_event->bstate |= BUTTON_SHIFT;
      }
      if (button & 8) {
        mouse_event->bstate |= BUTTON_ALT;
      }
      if (button & 16) {
        mouse_event->bstate |= BUTTON_CTRL;
      }

      if (base_button == 64) {
        mouse_event->bstate |= BUTTON4_PRESSED;
      }
      else if (base_button == 65) {
        mouse_event->bstate |= BUTTON5_PRESSED;
      }
      else if (base_button == 66) {
        mouse_event->bstate |= BUTTON6_PRESSED;
      }
      else if (base_button == 67) {
        mouse_event->bstate |= BUTTON7_PRESSED;
      }
      else if (base_button == 0) {
        if (terminator == 'M') {
          mouse_event->bstate |= BUTTON1_PRESSED;
        }
        else {
          mouse_event->bstate |= BUTTON1_RELEASED;
        }
      }
      else if (base_button == 1) {
        if (terminator == 'M') {
          mouse_event->bstate |= BUTTON2_PRESSED;
        }
        else {
          mouse_event->bstate |= BUTTON2_RELEASED;
        }
      }
      else if (base_button == 2) {
        if (terminator == 'M') {
          mouse_event->bstate |= BUTTON3_PRESSED;
        }
        else {
          mouse_event->bstate |= BUTTON3_RELEASED;
        }
      }
      else if (base_button >= 32 && base_button <= 35) {
        mouse_event->bstate |= REPORT_MOUSE_POSITION;
      }

      event->key_code = H_KEY_MOUSE;
      event->modifiers = 1;
      event->type = KITTY_EVENT_PRESS;
      return true;
    }
  }
  else if (terminator == 'M' && len == 0) {
    /* Legacy X10/Xterm mouse sequence: \033[M + 3 bytes */
    int b = getch();
    int x = getch();
    int y = getch();
    if (b != ERR && x != ERR && y != ERR) {
      mouse_event->x = x - 33;
      mouse_event->y = y - 33;
      mouse_event->z = 0;
      mouse_event->id = 0;
      mouse_event->bstate = 0;

      int button = b - 32;
      int base_button = button;
      if (button >= 64) {
        base_button = (button & 3) + 64;
      }
      else {
        base_button = button & (3 | 32);
      }

      if (button & 4) {
        mouse_event->bstate |= BUTTON_SHIFT;
      }
      if (button & 8) {
        mouse_event->bstate |= BUTTON_ALT;
      }
      if (button & 16) {
        mouse_event->bstate |= BUTTON_CTRL;
      }

      if (base_button == 64) {
        mouse_event->bstate |= BUTTON4_PRESSED;
      }
      else if (base_button == 65) {
        mouse_event->bstate |= BUTTON5_PRESSED;
      }
      else if (base_button == 66) {
        mouse_event->bstate |= BUTTON6_PRESSED;
      }
      else if (base_button == 67) {
        mouse_event->bstate |= BUTTON7_PRESSED;
      }
      else if (base_button == 0) {
        mouse_event->bstate |= BUTTON1_PRESSED;
      }
      else if (base_button == 1) {
        mouse_event->bstate |= BUTTON2_PRESSED;
      }
      else if (base_button == 2) {
        mouse_event->bstate |= BUTTON3_PRESSED;
      }
      else if (base_button == 3) {
        mouse_event->bstate |= BUTTON1_RELEASED;
      }
      else if (base_button >= 32 && base_button <= 35) {
        mouse_event->bstate |= REPORT_MOUSE_POSITION;
      }

      event->key_code = H_KEY_MOUSE;
      event->modifiers = 1;
      event->type = KITTY_EVENT_PRESS;
      return true;
    }
  }

  /* 1. Parse parameters manually to handle empty fields (e.g., [;5A) */
  int p[3] = {1, 1, 1};
  int p_idx = 0;
  char* ptr = buf;

  while (*ptr && p_idx < 3) {
    if (*ptr == ';' || *ptr == ':') {
      p_idx++;
      ptr++;
      continue;
    }
    if (isdigit((unsigned char)*ptr)) {
      p[p_idx] = atoi(ptr);
      while (isdigit((unsigned char)*ptr)) {
        ptr++;
      }
    }
    else {
      ptr++;
    }
  }

  int p1 = p[0], p2 = p[1], p3 = p[2];

  /* 2. Map terminators */
  event->modifiers = p2;
  event->type = (KittyEventType)p3;

  if (terminator == 'u') {
    event->key_code = p1;
    return true;
  }

  /* Map ANSI terminators */
  switch (terminator) {
    case 'A':
      event->key_code = 57352;
      break; /* Up */
    case 'B':
      event->key_code = 57353;
      break; /* Down */
    case 'C':
      event->key_code = 57351;
      break; /* Right */
    case 'D':
      event->key_code = 57350;
      break; /* Left */
    case 'H':
      event->key_code = 57356;
      break; /* Home */
    case 'F':
      event->key_code = 57357;
      break; /* End */
    case 'Z':
      event->key_code = 57346;
      event->modifiers = 2;
      break; /* Backtab */
    case '~':
      switch (p1) {
        case 2:
          event->key_code = 57348;
          break; /* Insert */
        case 3:
          event->key_code = 57349;
          break; /* Delete */
        case 5:
          event->key_code = 57354;
          break; /* PageUp */
        case 6:
          event->key_code = 57355;
          break; /* PageDown */
        case 7:
          event->key_code = 57356;
          break; /* Home */
        case 8:
          event->key_code = 57357;
          break; /* End */
        default:
          event->key_code = -2;
          break;
      }
      break;
    default:
      event->key_code = -2;
      break;
  }

  return true;
}

bool kitty_translate_event(const KittyKeyEvent* event, int* out_unified) {
  int key = event->key_code;
  if (key == H_KEY_MOUSE) {
    *out_unified = H_KEY_MOUSE;
    return true;
  }
  int mods = event->modifiers;

  int val = (mods > 0) ? mods - 1 : 0;
  bool shift = (val & 1) != 0;
  bool alt = (val & 2) != 0;
  bool ctrl = (val & 4) != 0;
  bool super = (val & 8) != 0;

  int unified_mods = 0;
  if (shift) {
    unified_mods |= K_MOD_SHIFT;
  }
  if (alt) {
    unified_mods |= K_MOD_ALT;
  }
  if (ctrl) {
    unified_mods |= K_MOD_CTRL;
  }
  if (super) {
    unified_mods |= K_MOD_SUPER;
  }

  /* Filter releases: pass only modifier key state changes or Ctrl releases */
  bool is_release = (event->type == KITTY_EVENT_RELEASE);
  if (is_release) {
    bool is_mod_key = (key >= 57441 && key <= 57452);
    bool is_ctrl_event = (key == 57443 || key == 57444);
    if (!is_mod_key && !is_ctrl_event) {
      return false;
    }
  }

  bool is_functional = false;
  int translated_key = key;

  if (key >= 57344 && key <= 57387) {
    is_functional = true;
    switch (key) {
      case 57344:
        translated_key = 27;
        break; /* Escape */
      case 57345:
        translated_key = KEY_ENTER;
        break; /* Enter */
      case 57346:
        translated_key = KEY_TAB;
        break;
      case 57347:
        translated_key = KEY_BACKSPACE;
        break;
      case 57348:
        translated_key = KEY_IC;
        break;
      case 57349:
        translated_key = KEY_DC;
        break;
      case 57350:
        translated_key = KEY_LEFT;
        break;
      case 57351:
        translated_key = KEY_RIGHT;
        break;
      case 57352:
        translated_key = KEY_UP;
        break;
      case 57353:
        translated_key = KEY_DOWN;
        break;
      case 57354:
        translated_key = KEY_PPAGE;
        break;
      case 57355:
        translated_key = KEY_NPAGE;
        break;
      case 57356:
        translated_key = KEY_HOME;
        break;
      case 57357:
        translated_key = KEY_END;
        break;
      default:
        if (key >= 57364 && key <= 57375) {
          translated_key = KEY_F(key - 57364 + 1);
        }
        break;
    }
  }
  else if (key >= 57399 && key <= 57427) {
    /* Numpad Mapping */
    switch (key) {
      case 57399:
        translated_key = '0';
        break;
      case 57400:
        translated_key = '1';
        break;
      case 57401:
        translated_key = '2';
        break;
      case 57402:
        translated_key = '3';
        break;
      case 57403:
        translated_key = '4';
        break;
      case 57404:
        translated_key = '5';
        break;
      case 57405:
        translated_key = '6';
        break;
      case 57406:
        translated_key = '7';
        break;
      case 57407:
        translated_key = '8';
        break;
      case 57408:
        translated_key = '9';
        break;
      case 57409:
        translated_key = '.';
        break;
      case 57410:
        translated_key = '/';
        break;
      case 57411:
        translated_key = '*';
        break;
      case 57412:
        translated_key = '-';
        break;
      case 57413:
        translated_key = '+';
        break;
      case 57414:
        translated_key = KEY_ENTER;
        is_functional = true;
        break;
      case 57415:
        translated_key = '=';
        break;
      case 57416:
        translated_key = ',';
        break;
      case 57417:
        translated_key = KEY_LEFT;
        is_functional = true;
        break;
      case 57418:
        translated_key = KEY_RIGHT;
        is_functional = true;
        break;
      case 57419:
        translated_key = KEY_UP;
        is_functional = true;
        break;
      case 57420:
        translated_key = KEY_DOWN;
        is_functional = true;
        break;
      case 57421:
        translated_key = KEY_PPAGE;
        is_functional = true;
        break;
      case 57422:
        translated_key = KEY_NPAGE;
        is_functional = true;
        break;
      case 57423:
        translated_key = KEY_HOME;
        is_functional = true;
        break;
      case 57424:
        translated_key = KEY_END;
        is_functional = true;
        break;
      case 57425:
        translated_key = 0x1B3;
        is_functional = true;
        break; /* KEY_B2 / Begin */
      case 57426:
        translated_key = KEY_IC;
        is_functional = true;
        break;
      case 57427:
        translated_key = KEY_DC;
        is_functional = true;
        break;
    }
  }
  else if (key == 27 || key == '\n' || key == '\r' || key == '\t' || key == 127 || key == 8) {
    is_functional = true;
    if (key == '\r' || key == '\n') {
      translated_key = KEY_ENTER;
    }
    else if (key == 127 || key == 8) {
      translated_key = KEY_BACKSPACE;
    }
  }

  if (translated_key == KEY_TAB && shift) {
    translated_key = KEY_BTAB;
  }

  if (is_functional) {
    *out_unified = K_SPECIAL(unified_mods, translated_key);
    if (is_release) {
      *out_unified |= K_FLAG_RELEASE;
    }
    return true;
  }

  if (ctrl || alt || super) {
    if (translated_key >= 'A' && translated_key <= 'Z') {
      translated_key += ('a' - 'A');
    }
    *out_unified = K_SPECIAL(unified_mods, translated_key);
    if (is_release) {
      *out_unified |= K_FLAG_RELEASE;
    }
  }
  else {
    if (shift && translated_key >= 'a' && translated_key <= 'z') {
      translated_key -= ('a' - 'A');
    }
    *out_unified = translated_key;
  }

  return true;
}
