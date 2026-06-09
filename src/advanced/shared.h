#ifndef WISHWIM_SHARED_H
#define WISHWIM_SHARED_H
#include "lsp/lsp_handler.h"
#include "tree-sitter/tree_manager.h"


typedef struct {
  TS_Data* ts_data;
  LSP_Data* lsp_data;
} PayloadStateChange;

PayloadStateChange getPayloadStateChange(TS_Data* highlight_datas, LSP_Data* lsp_data);

void globalOnStateChange(Action action, Cursor* cursor, void* payload_p);

#endif // WISHWIM_SHARED_H
