#ifndef WISHWIM_LSP_COMPLETION_H
#define WISHWIM_LSP_COMPLETION_H

#include "../../../data-management/file_management.h"
#include "../../../data-management/file_structure.h"
#include "../../../data-management/state_control.h"
#include "../../../environnement/global_variables.h"
#include "../../../io-management/viewport_history.h"
#include "../../shared.h"
#include "../lsp_client.h"

void executeLSPCompletion(LSP_Server* lsp, Cursor* cursor, LSP_CompletionItem* item, History** history_p,
                          PayloadStateChange *payload_state_change, LF_Tabulation* tab);


void askCompletion(gui_Context* gui_context, FileContainer* fc, bool reset, bool force);

void receiveCompletionData(cJSON* packet, FileContainer* file, ViewPort* view_port, Cursor* cursor);

#endif // WISHWIM_LSP_COMPLETION_H
