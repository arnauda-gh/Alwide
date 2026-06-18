#ifndef KEY_MANAGEMENT_H
#define KEY_MANAGEMENT_H

#include <ncurses.h>

/**
 * @file key_management.h
 * @brief Unified Input Pipeline & Bit-Packed Key System
 *
 * --- 1. Bit-Packed Key Implementation ---
 * All terminal input (Kitty Protocol, ANSI escapes, and Legacy bytes) is unified into
 * a single 32-bit 'int key' using the following bitfield layout:
 *
 * Bits 0-23  [24b] : Codepoint / Keycode (Unicode character or Ncurses KEY_*)
 * Bit 24     [ 1b] : Shift Modifier (K_MOD_SHIFT)
 * Bit 25     [ 1b] : Alt Modifier (K_MOD_ALT)
 * Bit 26     [ 1b] : Ctrl Modifier (K_MOD_CTRL)
 * Bit 27     [ 1b] : Super (Windows/Cmd) Modifier (K_MOD_SUPER)
 * Bit 28     [ 1b] : Special Flag (K_FLAG_SPECIAL):
 *                    1 = Action/Command (Routing to runSpecialKeyHandler)
 *                    0 = Character (Routing to handleCharBufferInsertion)
 * Bit 29     [ 1b] : Release Flag (K_FLAG_RELEASE):
 *                    1 = Key Release event
 *                    0 = Key Press/Repeat event
 *
 * --- 2. Input Workflow ---
 * The main loop in src/main.c orchestrates the following flow:
 *
 *   [Main Loop] -> calls readNextInput(&ctx)
 *      |
 *      +--> [Step A: getch()]
 *      |    Polls Ncurses input stream (timeout: 20ms).
 *      |
 *      +--> [Step B: Kitty/ANSI Parsing]
 *      |    If byte is ESC (27), enters kitty_parse_sequence().
 *      |    - Modern (Kitty): Parses CSI ... u sequences for precise modifier/release info.
 *      |    - ANSI: Parses legacy CSI (e.g., [A) for standard arrows/function keys.
 *      |    - Returns a unified 'int key'.
 *      |
 *      +--> [Step C: UTF-8 Manual Decoding]
 *      |    If Step B fails and byte has bit 7 set (>=128), manually decodes
 *      |    multibyte UTF-8 sequences from the Ncurses stream to prevent IO corruption.
 *      |
 *      +--> [Step D: Legacy Normalization]
 *      |    If Step C fails, calls normalize_legacy().
 *      |    - Maps ASCII Controls (1-31) to K_MOD_CTRL | [char].
 *      |    - Maps Ncurses keycodes (KEY_UP, etc.) to Special Unified Keys.
 *      |
 *   [Main Loop] -> calls dispatchInput(ctx, key)
 *      |
 *      +--> [Routing]
 *           - If K_IS_SPECIAL(key): routes to runSpecialKeyHandler (Commands).
 *           - Else: routes to handleCharBufferInsertion (Typing).
 *
 * --- 3. Fallback Mechanism ---
 * 1. Tier 1 (Kitty Protocol): Primary choice. Provides full modifier state and release
 *    events for all keys. Active if the terminal supports it.
 * 2. Tier 2 (ANSI Sequences): Fallback for standard terminals. Handles simple
 *    combinations (e.g., Shift+Arrow) using Ncurses translation.
 * 3. Tier 3 (ASCII/Legacy): Final fallback. Maps Ctrl+Key (1-26) and raw bytes.
 *
 */

#define K_MOD_NONE     0
#define K_MOD_SHIFT    (1 << 24)
#define K_MOD_ALT      (1 << 25)
#define K_MOD_CTRL     (1 << 26)
#define K_MOD_SUPER    (1 << 27)
#define K_FLAG_SPECIAL (1 << 28)
#define K_FLAG_RELEASE (1 << 29)

/*
 * API for the Unified Key System
 * Use these macros to interact with bit-packed 'int key' values.
 */
#define K_CODE(key)       ((key) & 0x00FFFFFF)
#define K_HAS_CTRL(key)   ((key) & K_MOD_CTRL)
#define K_HAS_ALT(key)    ((key) & K_MOD_ALT)
#define K_HAS_SHIFT(key)  ((key) & K_MOD_SHIFT)
#define K_HAS_SUPER(key)  ((key) & K_MOD_SUPER)
#define K_IS_SPECIAL(key) ((key) & K_FLAG_SPECIAL)
#define K_IS_RELEASE(key) ((key) & K_FLAG_RELEASE)
#define K_IS_PRESS(key)   (!K_IS_RELEASE(key))

/* Macros for defining and checking keys */
#define K_SPECIAL(mods, key)         ((mods) | (key) | K_FLAG_SPECIAL)
#define K_SPECIAL_RELEASE(mods, key) (K_SPECIAL(mods, key) | K_FLAG_RELEASE)

#define TIME_BETWEEN_EVENT 200 /*ms*/

// --- Unified Key Binds ---

#define H_KEY_RESIZE      K_SPECIAL(0, KEY_RESIZE)
#define H_KEY_MOUSE       K_SPECIAL(0, KEY_MOUSE)
#define H_KEY_ESCAPE      K_SPECIAL(0, 27)
#define KEY_TAB           '\t'
#define H_KEY_TAB         K_SPECIAL(0, KEY_TAB)
#define H_KEY_SHIFT_TAB   K_SPECIAL(K_MOD_SHIFT, KEY_BTAB)
#define H_KEY_ENTER       K_SPECIAL(0, KEY_ENTER)
#define H_KEY_SHIFT_ENTER K_SPECIAL(K_MOD_SHIFT, KEY_ENTER)
#define H_KEY_BACKSPACE   K_SPECIAL(0, KEY_BACKSPACE)

#define H_KEY_DELETE H_KEY_BACKSPACE /* Alias for consistency */

#define H_KEY_SUPPR       K_SPECIAL(0, KEY_DC)
#define H_KEY_CTRL_SUPPR  K_SPECIAL(K_MOD_CTRL, KEY_DC)
#define H_KEY_CTRL_DELETE K_SPECIAL(K_MOD_CTRL, KEY_BACKSPACE)

#define H_KEY_RIGHT K_SPECIAL(0, KEY_RIGHT)
#define H_KEY_LEFT  K_SPECIAL(0, KEY_LEFT)
#define H_KEY_UP    K_SPECIAL(0, KEY_UP)
#define H_KEY_DOWN  K_SPECIAL(0, KEY_DOWN)

#define H_KEY_MAJ_RIGHT K_SPECIAL(K_MOD_SHIFT, KEY_RIGHT)
#define H_KEY_MAJ_LEFT  K_SPECIAL(K_MOD_SHIFT, KEY_LEFT)
#define H_KEY_MAJ_UP    K_SPECIAL(K_MOD_SHIFT, KEY_UP)
#define H_KEY_MAJ_DOWN  K_SPECIAL(K_MOD_SHIFT, KEY_DOWN)

#define H_KEY_CTRL_RIGHT     K_SPECIAL(K_MOD_CTRL, KEY_RIGHT)
#define H_KEY_CTRL_LEFT      K_SPECIAL(K_MOD_CTRL, KEY_LEFT)
#define H_KEY_CTRL_DOWN      K_SPECIAL(K_MOD_CTRL, KEY_DOWN)
#define H_KEY_CTRL_UP        K_SPECIAL(K_MOD_CTRL, KEY_UP)
#define H_KEY_CTRL_SEMICOLON K_SPECIAL(K_MOD_CTRL, ';')

#define H_KEY_CTRL_MAJ_RIGHT K_SPECIAL(K_MOD_CTRL | K_MOD_SHIFT, KEY_RIGHT)
#define H_KEY_CTRL_MAJ_LEFT  K_SPECIAL(K_MOD_CTRL | K_MOD_SHIFT, KEY_LEFT)
#define H_KEY_CTRL_MAJ_DOWN  K_SPECIAL(K_MOD_CTRL | K_MOD_SHIFT, KEY_DOWN)
#define H_KEY_CTRL_MAJ_UP    K_SPECIAL(K_MOD_CTRL | K_MOD_SHIFT, KEY_UP)

#define H_KEY_BEGIN   K_SPECIAL(0, KEY_HOME)
#define H_KEY_END     K_SPECIAL(0, KEY_END)
#define H_KEY_MAJ_END K_SPECIAL(K_MOD_SHIFT, KEY_END)
#define H_KEY_F12     KEY_F(12)

#define ONLY_REPAINT_INPUT K_SPECIAL(0, 0x0FFFFFFF) /* Unique value outside standard range */


// --- Mouse Binds ---

#define BEGIN_MOUSE_LISTEN 588
#define MOUSE_IN_OUT       589

#ifndef BUTTON6_PRESSED
#define BUTTON6_PRESSED 0x40000000
#endif
#ifndef BUTTON7_PRESSED
#define BUTTON7_PRESSED 0x80000000
#endif
#ifndef BUTTON8_PRESSED
#define BUTTON8_PRESSED 0x100000000ULL
#endif
#ifndef BUTTON9_PRESSED
#define BUTTON9_PRESSED 0x200000000ULL
#endif


void detectComplexMouseEvents(MEVENT* event);
void printEventList(MEVENT* event);

/* Key Processing and Logging */
int normalize_legacy(int c);
void logInput(int key);

#endif // KEY_MANAGEMENT_H
