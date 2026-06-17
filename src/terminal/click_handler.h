#ifndef CLICK_HANDLER_H
#define CLICK_HANDLER_H

#include "../core/editor_context.h"
#include "../data-management/file_management.h"
#include "../data-management/file_structure.h"
#include "../io-management/io_explorer.h"
#include "term_handler.h"

#include "../advanced/lsp/lsp_dispatcher.h"

bool isClickInsideWindow(WINDOW* w, MEVENT* m_event);

void handleClick(EditorContext* ctx);

////// -------------- CLICK FUNCTIONS --------------

void handleEditorClick(gui_Context* gui_context, Cursor* cursor, Cursor* select_cursor, int* desired_column,
                       int* screen_x, int* screen_y, MEVENT* m_event, bool mouse_drag, FileContainer* file,
                       WindowHighlightDescriptor* highlight_descriptor);

void handleOpenedFileClick(gui_Context* gui_context, FileContainer* files, int* file_count, int* current_file,
                           MEVENT m_event, bool* refresh_local_vars, bool mouse_drag);

void handleFileExplorerClick(EditorContext *ctx);

// true if found, false if not. if file_index == -1 => Res_folder was clicked. If file_index != -1 => file clicked is
// res_folder.files[file_index].
bool getFileClickedFileExplorer(ExplorerFolder* pwd, int y_click, int few_x_offsset, int few_y_offset,
                                ExplorerFolder** res_folder, int* file_index);


bool dispatchInputToTPW(EditorContext* ctx, int key);

#endif // CLICK_HANDLER_H
