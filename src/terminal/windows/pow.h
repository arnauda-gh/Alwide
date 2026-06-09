#ifndef WISHWIM_POW_H
#define WISHWIM_POW_H
#include "gui_entities.h"

#include "../../io-management/viewport_history.h"

bool gui_resumeHoverInformation(Cursor* cursor, ViewPort* view_port, FileContainer* file, LSP_Hover* hover,
                                int tab_size);

bool gui_resumeGotoChoice(ViewPort* view_port, Cursor* cursor, int tab_size);

bool gui_resumeCompletionTextAnchor(ViewPort* view_port, Cursor* cursor, int tab_size);

void gui_showGenericPopupWithTextAnchor(ViewPort* view_port, Cursor* cursor, int height, int width,
                                        PopupOwner popup_owner, int tab_size);

void gui_showGenericPopup(gui_Context* gui_context, int y, int x, int prefered_height, int prefered_width,
                          PopupOwner popup_owner);

void gui_setLastTextAnchor(gui_Context* gui_context, CursorDescriptor descriptor);

void gui_showDiagnostic(gui_Context* gui_context, int y, int x, LSP_Diagnostic* diagnostic, int tab_size);

void gui_printPopup(gui_EDW* context, Cursor* cursor, LSP_ComputedData* lsp_data, int tab_size);

struct ModuleContext;
typedef struct ModuleContext ModuleContext;

#include "../../data-management/file_management.h"

bool gui_handlePopupInput(gui_Context* context, FileContainer* fc, int key, PayloadStateChange* payload_state_change,
                          ModuleContext* payload, MEVENT* m_event);

#endif // WISHWIM_POW_H
