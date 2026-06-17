#ifndef WISHWIM_FTW_H
#define WISHWIM_FTW_H

#include "../../advanced/lsp/lsp_handler.h"
#include "../../data-management/file_management.h"
#include "../highlight.h"
#include "gui_entities.h"

void gui_initEDWContext(gui_EDW* context);

void gui_resizeEDW(gui_Context* gui_context, int lnw_new_width);

void gui_repaintEDW(gui_EDW* context, FileContainer* fc, WindowHighlightDescriptor* highlight_descriptor);

void gui_repaintSBW(gui_EDW* context, FileContainer* fc);

void gui_switchSBW(gui_Context* gui_context);

int getEDW_LengthLineNumber(gui_Context* gui_context);

bool gui_showEDWPopup(gui_Context* gui_context, int y, int x, int height, int width, PopupOwner owner);

void gui_closeEDWPopup(gui_Context* gui_context);

bool gui_adaptEDWPopup(gui_Context* gui_context, int slice_x, int slice_y);

#endif // WISHWIM_FTW_H
