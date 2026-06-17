#include "lsp_hover.h"

#include <assert.h>

#include "../../../terminal/term_handler.h"
#include "../../../terminal/windows/edw.h"
#include "../../../terminal/windows/pow.h"
#include "../lsp-features/lsp_completion.h"


void receiveHoverData(cJSON* packet, FileContainer* file, ViewPort* view_port, Cursor* cursor, void* payload) {
  LSP_Hover hover;
  LSP_getHoverFromJSON(LSP_getPacketResult(packet), &hover);
  assert(payload != NULL);

  // if there is no hover data we don't override the current data and we close the popup.
  if (hover.size == 0) {
    gui_closeEDWPopup(view_port->gui);
    free(payload);
    return;
  }
  LSP_destroyHover(&file->lsp_datas.computed->hover);
  file->lsp_datas.computed->hover = hover;

  // create range using cursor position when request if not present
  if (file->lsp_datas.computed->hover.size > 0 && file->lsp_datas.computed->hover.is_range == false) {
    LSP_Position* pos = payload;
    LSP_Server* lsp = getLSPServerForLanguage(&lsp_servers, file->lsp_datas.lang_id);

    Cursor end = LSP_tryToReachCursorForLSPPosition(lsp, *cursor, *pos);
    Cursor begin = cursor_disable(*cursor);
    selectWord(&end, &begin);

    // set up the new range
    file->lsp_datas.computed->hover.range.pos1 = LSP_pos_from_cursor(lsp, begin);
    file->lsp_datas.computed->hover.range.pos2 = LSP_pos_from_cursor(lsp, end);
    file->lsp_datas.computed->hover.is_range = true;
  }

  // We assert that every data going outfrom there has a range.
  assert(file->lsp_datas.computed->hover.is_range);
  gui_resumeHoverInformation(cursor, view_port, file, &file->lsp_datas.computed->hover, LF_tab_size(file->feature));

  free(payload);
}
