#ifndef KITTY_KEYBOARD_H
#define KITTY_KEYBOARD_H

#include <ncurses.h>
#include <stdbool.h>
#include <stddef.h>

/* Event types reported by the Kitty keyboard protocol */
typedef enum { KITTY_EVENT_PRESS = 1, KITTY_EVENT_REPEAT = 2, KITTY_EVENT_RELEASE = 3 } KittyEventType;

/* Representation of a parsed Kitty keyboard event */
typedef struct {
  int key_code;        /* Unicode codepoint or Kitty functional keycode */
  int modifiers;       /* Modifier bitmask (1 + Shift=1, Alt=2, Ctrl=4, Super=8) */
  KittyEventType type; /* Press, Repeat, or Release */
} KittyKeyEvent;

/**
 * Enables the Kitty Keyboard Protocol in the terminal.
 * Sends the sequence to set flags 1 (disambiguate) and 2 (event types).
 */
void kitty_enable(void);

/**
 * Disables the Kitty Keyboard Protocol in the terminal.
 * Resets the terminal's keyboard mode back to legacy standard.
 */
void kitty_disable(void);

/**
 * Parses a CSI u / Kitty Keyboard sequence from the stdin queue.
 * Should be called when a leading Escape byte (27) is read.
 *
 * @param first_char The initial character read (should be 27/Escape).
 * @param event Pointer to a KittyKeyEvent structure to populate.
 * @param mouse_event Pointer to an MEVENT structure to populate if SGR mouse is parsed.
 * @param out_unread Pointer to write a peeked byte that should be "unread" (if not a sequence).
 * @return true if a valid sequence (or raw legacy ESC) was successfully parsed, false otherwise.
 */
bool kitty_parse_sequence(int first_char, KittyKeyEvent* event, MEVENT* mouse_event, int* out_unread);


/**
 * Translates a parsed Kitty keyboard event into a unified bit-packed value.
 * Maps functional keys directly to ncurses codes with modifier bits.
 *
 * @param event Pointer to the parsed KittyKeyEvent.
 * @param out_unified Pointer to write the translated unified key value to.
 * @return true if the event should be processed, false if it should be ignored.
 */
bool kitty_translate_event(const KittyKeyEvent* event, int* out_unified);

#endif /* KITTY_KEYBOARD_H */
