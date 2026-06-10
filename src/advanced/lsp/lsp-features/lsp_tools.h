#ifndef ALWIDE_LSP_TOOLS_H
#define ALWIDE_LSP_TOOLS_H

#include "../../../data-management/file_management.h"
#include "../../../data-management/file_structure.h"
#include "../../../data-management/state_control.h"
#include "../../shared.h"
#include "../lsp_client.h"

void applyTextEdit(LSP_Server* lsp, Cursor* cursor, LSP_TextEdit* text_edit, History** history_p,
                   PayloadStateChange *payload_state_change, LF_Tabulation* tab);


/**
 * Applies an array of text edits to the current cursor/file.
 * Edits are sorted bottom-to-top to ensure offsets remain valid during application.
 * The cursor position is tracked and updated based on the shifts.
 */
void applyTextEditsArray(LSP_Server* lsp, Cursor* cursor, LSP_TextEdit* edits, int edits_size, History** history_p,
                         PayloadStateChange *payload_state_change, LF_Tabulation* tab);

/**
 * Applies a full WorkspaceEdit.
 * Currently primarily supports changes to the active file (fc).
 */
void applyWorkspaceEdit(FileContainer* fc, Cursor* cursor, LSP_WorkspaceEdit* ws_edit,
                        PayloadStateChange* payload_state_change);

int compareTextEdit(const void* e1_p, const void* e2_p);

int compareLSPPos(LSP_Position p1, LSP_Position p2);

LSP_Position calculateEndPos(LSP_Server* server, LSP_Position start, const char* text);


#endif // ALWIDE_LSP_TOOLS_H
