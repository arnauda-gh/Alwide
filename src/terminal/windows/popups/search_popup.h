#ifndef ALWIDE_SEARCH_POPUP_H
#define ALWIDE_SEARCH_POPUP_H

#include "../../../core/editor_state.h"

/**
 * Spawns the graphical search popup positioned at the bottom of the screen.
 * Captures focalized input and communicates search queries to the intelligence search module.
 */
void gui_openSearchPopup(EditorContext* ctx, char* query);

#endif // ALWIDE_SEARCH_POPUP_H
