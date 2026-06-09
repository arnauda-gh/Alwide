#include "file_management.h"

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "../config/language_feature.h"
#include "../environnement/global_variables.h"
#include "../io-management/viewport_history.h"
#include "../terminal/key_management.h"
#include "../terminal/term_handler.h"
#include "../utils/clipboard_manager.h"
#include "encoding/utf8.h"

////// -------------- FILE CONTAINER --------------


void destroyFileContainer(FileContainer* container) {
  if (container->io_file.status == EXIST) {
    setlastFilePosition(container->io_file.path_abs, container->cursor.file_id.absolute_row,
                        container->cursor.line_id.absolute_column, container->screen_x, container->screen_y);
  }
  destroyFullFile(container->root);
  destroyEndOfHistory(container->history_root);
  free(container->history_root);
  ts_tree_delete(container->highlight_data.tree);
  destroyLspDatas(&container->lsp_datas);
}

void openNewFile(char* file_path, FileContainer** files, int* file_count, int* current_file, bool* refresh_ofw,
                 bool* refresh_local_vars) {
  // Update editors vars
  *refresh_local_vars = true;
  *refresh_ofw = true;

  char abs_path[PATH_MAX];
  char* res = realpath(file_path, abs_path);
  // Check for an existing buffer.
  if (res != NULL) {
    for (int i = 0; i < *file_count; i++) {
      if ((*files)[i].io_file.status != NONE && strcmp((*files)[i].io_file.path_abs, abs_path) == 0) {
        *current_file = i;
        return;
      }
    }
  }

  // Reallocate files to handle one more file.
  (*file_count)++;
  *files = realloc(*files, *file_count * sizeof(FileContainer));

  // Setup new file container with clicked file
  setupFileContainer(file_path, *files + *file_count - 1);

  if ((*files)[*file_count - 1].lsp_datas.is_enable) {
    char* dump = dumpSelection(tryToReachAbsPosition((*files)[*file_count - 1].cursor, 1, 0),
                               tryToReachAbsPosition((*files)[*file_count - 1].cursor, INT_MAX, INT_MAX));
    LSP_notifyLspFileDidOpen(getLSPServerForLanguage(&lsp_servers, (*files)[*file_count - 1].lsp_datas.lang_id),
                             (*files)[*file_count - 1].io_file.path_abs, dump);
    free(dump);
  }

  *current_file = *file_count - 1;
}

void closeFile(FileContainer** files, int* file_count, int* current_file, bool* refresh_local_vars) {
  if (*file_count == 1) // Always need to keep one file.
  {
    return;
  }
  // Clear current file
  destroyFileContainer(*files + *current_file);
  memmove(*files + *current_file, *files + *current_file + 1,
          (*file_count - *current_file - 1) * sizeof(FileContainer));

  // Change vars
  if (*current_file != 0) {
    (*current_file)--;
  }
  (*file_count)--;

  // realloc files to avoid excess of mem.
  *files = realloc(*files, *file_count * sizeof(FileContainer));
  *refresh_local_vars = true;
}

Cursor createRoot(IO_FileID file, LF_Tabulation* tab) {
  if (file.status == EXIST) {
    return initWrittableFileFromFile(file.path_abs, tab);
  }
  return initNewWrittableFile();
}

void setupFileContainer(char* path, FileContainer* container) {
  setupFile(path, &container->io_file);
  container->screen_x = 1;
  container->screen_y = 1;
  container->old_screen_x = -1;
  container->old_screen_y = -1;
  container->history_root = malloc(sizeof(History));
  container->history_frame = container->history_root;

  container->feature = LF_getFeatureForFile(container->io_file.path_abs);

  setFileHighlightDatas(&container->highlight_data, container->feature);
  setLspDatas(&container->lsp_datas, container->io_file, container->feature);

  container->cursor = createRoot(container->io_file, LF_tab(container->feature));
  container->select_cursor = cursor_disable(container->cursor);
  setDesiredColumn(container->cursor, &container->desired_column);

  container->root = container->cursor.file_id.file;
  assert(container->root->prev == NULL);
  fetchSavedCursorPosition(container->io_file, &container->cursor, &container->screen_x, &container->screen_y);
  container->old_cur = container->cursor; // set the old_cursor after
  loadCurrentStateControl(container->history_root, &container->history_frame, container->io_file);
}


void setupLocalVars(FileContainer* files, int current_file, IO_FileID** io_file, FileNode*** root, Cursor** cursor,
                    Cursor** select_cursor, Cursor** old_cur, int** desired_column, int** screen_x, int** screen_y,
                    int** old_screen_x, int** old_screen_y, History*** history_root, History*** history_frame,
                    TS_Data** highlight_data, LSP_Data** lsp_datas, LF_LanguageFeature** feature) {
  *io_file = &files[current_file].io_file;               // Describe the IO file on OS
  *root = &files[current_file].root;                     // The root of the File object
  *cursor = &files[current_file].cursor;                 // The current cursor for the root File
  *select_cursor = &files[current_file].select_cursor;   // The cursor used to make selection
  *old_cur = &files[current_file].old_cur;               // Old cursor used to flag cursor change
  *desired_column = &files[current_file].desired_column; // Used on line change to try to reach column
  *screen_x = &files[current_file].screen_x; // The x coord of the top left corner of the current viewport of the file
  *screen_y = &files[current_file].screen_y; // The y coord of the top left corner of the current viewport of the file
  *old_screen_x = &files[current_file].old_screen_x;   // old screen_x used to flag screen_x changes
  *old_screen_y = &files[current_file].old_screen_y;   // old screen_y used to flag screen_y changes
  *history_root = &files[current_file].history_root;   // Root of History object for the current File
  *history_frame = &files[current_file].history_frame; // Current node of the History. Before -> Undo, After -> Redo.
  *highlight_data =
    &files[current_file].highlight_data;       // Object which represent the highlight data of the current file.
  **old_screen_y = -1;                         // Reset old_screen_y to force a viewport diff on next render.
  *lsp_datas = &files[current_file].lsp_datas; // Object which contains all the data of the active LSP server.
  *feature =
    files[current_file].feature; // Language feature config detected for this file (tabs, pairs, LSP, comments).
}


bool isFileContainerEmpty(FileContainer* container) {
  if (container->io_file.status != NONE) {
    return false;
  }

  return container->cursor.file_id.absolute_row == 1 && container->cursor.line_id.absolute_column == 0 &&
         cursor_eq(container->cursor, moveRight(container->cursor));
}

void setupOpenedFiles(int* file_count, char** file_names, FileContainer** files) {
  // allocating space for file at open.
  *files = malloc(sizeof(FileContainer) * max(1, *file_count));


  // Fill files with args
  for (int i = 0; i < *file_count; i++) {
    setupFileContainer(file_names[i], *files + i);

    if ((*files)[i].lsp_datas.is_enable) {
      char* dump = dumpSelection(tryToReachAbsPosition((*files)[i].cursor, 1, 0),
                                 tryToReachAbsPosition((*files)[i].cursor, INT_MAX, INT_MAX));
      LSP_notifyLspFileDidOpen(getLSPServerForLanguage(&lsp_servers, (*files)[i].lsp_datas.lang_id),
                               (*files)[i].io_file.path_abs, dump);
      free(dump);
    }
  }
  // If no args setup first untitled file.
  if (*file_count == 0) {
    *file_count = 1;
    setupFileContainer("", *files);
    if ((*files)[0].lsp_datas.is_enable) {
      LSP_notifyLspFileDidOpen(getLSPServerForLanguage(&lsp_servers, (*files)[0].lsp_datas.lang_id),
                               (*files)[0].io_file.path_abs, "");
    }
  }
}

FilesState filesStateOf(FileContainer** files, int* size, int* current_file_index, bool* refresh_local_vars) {
  return (FilesState){
    .files = files, .size = size, .current_file_index = current_file_index, .refresh_local_vars = refresh_local_vars};
}

//// -------------- CURSOR MANAGEMENT --------------

Cursor moveRight(Cursor cursor) {
  if (hasElementAfterLine(cursor.line_id)) {
    cursor.line_id.relative_column++;
    cursor.line_id.absolute_column++;
    cursor = moduloCursor(cursor);
  }
  else if (hasElementAfterFile(cursor.file_id)) {
    // If cursor is at the end of the line try to go on next line.
    cursor.file_id.relative_row++;
    cursor.file_id.absolute_row++;
    cursor = cursorOf(cursor.file_id, moduloLineIdentifierR(getLineForFileIdentifier(cursor.file_id), 0));
  }

  return cursor;
}

Cursor moveLeft(Cursor cursor) {
  if (hasElementBeforeLine(cursor.line_id)) {
    cursor.line_id.relative_column--;
    cursor.line_id.absolute_column--;
    cursor = moduloCursor(cursor);
  }
  else if (hasElementBeforeFile(cursor.file_id)) {
    // If the cursor is at the begin of the line try to reach previous line.
    cursor.file_id.relative_row--;
    cursor.file_id.absolute_row--;
    cursor = cursorOf(cursor.file_id, moduloLineIdentifierR(getLineForFileIdentifier(cursor.file_id),
                                                            sizeLineNode(getLineForFileIdentifier(cursor.file_id))));
  }

  return cursor;
}

Cursor moveUp(Cursor cursor, int desiredColumn) {
  if (hasElementBeforeFile(cursor.file_id)) {
    cursor.file_id.relative_row--;
    cursor.file_id.absolute_row--;
    cursor =
      cursorOf(cursor.file_id,
               tryToReachAbsColumn(moduloLineIdentifierR(getLineForFileIdentifier(cursor.file_id), 0), desiredColumn));
  }

  return cursor;
}

Cursor moveDown(Cursor cursor, int desiredColumn) {
  if (hasElementAfterFile(cursor.file_id)) {
    cursor.file_id.relative_row++;
    cursor.file_id.absolute_row++;
    cursor =
      cursorOf(cursor.file_id,
               tryToReachAbsColumn(moduloLineIdentifierR(getLineForFileIdentifier(cursor.file_id), 0), desiredColumn));
  }

  return cursor;
}

Cursor deleteCharAtCursor(Cursor cursor) {
  if (cursor.line_id.absolute_column == 0) {
    if (cursor.file_id.absolute_row != 1) {
      cursor = concatNeighbordsLinesC(cursor);
    }
  }
  else {
    cursor = removeCharInLineC(cursor);
  }
  return cursor;
}

Cursor supprCharAtCursor(Cursor cursor) {
  Cursor old_cur = cursor;
  cursor = moveRight(cursor);
  if (old_cur.file_id.absolute_row != cursor.file_id.absolute_row ||
      old_cur.line_id.absolute_column != cursor.line_id.absolute_column) {
    cursor = deleteCharAtCursor(cursor);
  }
  return cursor;
}

Cursor deleteLineAtCursor(Cursor cursor) {
  if (cursor.file_id.absolute_row != 1 || hasElementAfterFile(cursor.file_id)) {
    cursor = removeLineInFileC(cursor);
  }
  else {
    destroyFullLine(cursor.line_id.line);
    cursor.line_id = moduloLineIdentifierR(getLineForFileIdentifier(cursor.file_id), 0);
  }
  return cursor;
}


Cursor skipRightInvisibleChar(Cursor cursor) {
  Cursor tmp_cur;
  while (hasElementAfterLine(cursor.line_id) &&
         isInvisible(getCharForLineIdentifier((tmp_cur = moveRight(cursor)).line_id))) {
    cursor = tmp_cur;
  }

  return cursor;
}

Cursor skipLeftInvisibleChar(Cursor cursor) {
  while (cursor.line_id.absolute_column != 0 && isInvisible(getCharForLineIdentifier(cursor.line_id))) {
    cursor = moveLeft(cursor);
  }

  return cursor;
}

Cursor moveToNextWord(Cursor cursor) {
  cursor = skipRightInvisibleChar(cursor);
  Cursor old_cur = cursor;

  Char_U8 repeated;
  if (hasElementAfterLine(cursor.line_id)) {
    repeated = getCharForLineIdentifier(moveRight(cursor).line_id);
  }

  Cursor tmp_cur;
  bool canWorkWithLetter = true;
  while (hasElementAfterLine(cursor.line_id) &&
         ((canWorkWithLetter =
             isAWordLetter(getCharForLineIdentifier((tmp_cur = moveRight(cursor)).line_id)) && canWorkWithLetter) ||
          areChar_U8Equals(getCharForLineIdentifier(tmp_cur.line_id), repeated))) {
    cursor = tmp_cur;
  }

  if (cursor_eq(old_cur, cursor)) {
    return moveRight(cursor);
  }

  return cursor;
}

Cursor moveToPreviousWord(Cursor cursor) {
  cursor = skipLeftInvisibleChar(cursor);
  Cursor old_cur = cursor;

  Char_U8 repeated;
  if (cursor.line_id.absolute_column != 0) {
    repeated = getCharForLineIdentifier(cursor.line_id);
  }

  bool canWorkWithLetter = true;
  while (cursor.line_id.absolute_column != 0 &&
         ((canWorkWithLetter = isAWordLetter(getCharForLineIdentifier(cursor.line_id)) && canWorkWithLetter) ||
          areChar_U8Equals(getCharForLineIdentifier(cursor.line_id), repeated))) {
    cursor = moveLeft(cursor);
  }

  if (cursor_eq(old_cur, cursor)) {
    return moveLeft(cursor);
  }

  return cursor;
}

Cursor insertCharArrayAtCursor(Cursor cursor, char* chs, LF_Tabulation* tab) {
  // Duplicated search in project DUP_SCAN.

  int index = 0;

  char c;
  while ((c = chs[index++]) != '\0') {
#ifdef LOGS
    // assert(checkFileIntegrity(root) == true);
#endif
    if (iscntrl(c)) {
      if (c == '\n') {
#ifdef LOGS
        // printf("Enter\r\n");
#endif
        cursor = insertNewLineInLineC(cursor);
      }
      else if (c == 9) {
#ifdef LOGS
        // printf("Tab\r\n");
#endif
        Char_U8 ch;
        if (!tab->use_space) {
          ch.t[0] = '\t';
          cursor = insertCharInLineC(cursor, ch);
        }
        else {
          ch.t[0] = ' ';
          for (int i = 0; i < tab->size; i++) {
            cursor = insertCharInLineC(cursor, ch);
          }
        }
      }
      else {
#ifdef LOGS
        // printf("Unsupported Char loaded from file : '%d'.\r\n", c);
#endif
        // exit(0);
      }
    }
    else {
      Char_U8 ch = readChar_U8FromCharArrayWithFirst(chs + index - 1, c);
      index += sizeChar_U8(ch) - 1;
#ifdef LOGS
      printChar_U8(stdout, ch);
      // printf("\r\n");
#endif
      cursor = insertCharInLineC(cursor, ch);
    }
  }


  return cursor;
}

Cursor insertCharArrayAtCursorWithState(History** history_p, Cursor cursor, char* chs,
                                        PayloadStateChange payload_state_change, LF_Tabulation* tab) {
  Cursor tmp = cursor;
  cursor = insertCharArrayAtCursor(cursor, chs, tab);
  saveAction(history_p, createInsertAction(tmp, cursor_to_desc(cursor)), globalOnStageChange, &cursor,
             &payload_state_change);

  return cursor;
}

Cursor byteCursorToCursor(Cursor cursor, int row, int byte_column) {
  row += 1;
  cursor = tryToReachAbsPosition(cursor, row, 0);

  int byte_count = 0;
  while (byte_count < byte_column) {
    Cursor old_cur = cursor;
    cursor = moveRight(cursor);
    if (old_cur.file_id.absolute_row != cursor.file_id.absolute_row || cursor_eq(old_cur, cursor)) {
      return old_cur;
    }
    byte_count += sizeChar_U8(getCharAtCursor(cursor));
  }
  return cursor;
}


Cursor goToEnd(Cursor cursor) {
  cursor = cursorOf(cursor.file_id, tryToReachAbsColumn(cursor.line_id, INT_MAX));
  return cursor;
}

Cursor goToBegin(Cursor cursor) {
  cursor = cursorOf(cursor.file_id, tryToReachAbsColumn(cursor.line_id, 0));
  return cursor;
}

////// -------------- SELECTION MANAGEMENT --------------


int utf8CharBetween2Cursor(Cursor cur1, Cursor cur2) {
  if (cursor_eq(cur1, cur2)) {
    return 0;
  }
  if (cursor_le(cur2, cur1)) {
    Cursor tmp = cur1;
    cur1 = cur2;
    cur2 = tmp;
  }

  int count = 0;
  while (cursor_lt(cur1, cur2)) {
    Cursor next = moveRight(cur1);
    if (cursor_eq(cur1, next)) {
      break;
    }
    cur1 = next;
    count++;
  }
  return count;
}

unsigned int byteBetween2Cursor(Cursor cur1, Cursor cur2) {
  if (cursor_le(cur2, cur1)) {
    Cursor tmp = cur1;
    cur1 = cur2;
    cur2 = tmp;
  }

  Cursor it = cur1;
  int byte_between_2_cursor = 0;
  while (cursor_lt(it, cur2)) {
    Cursor tmp = it;
    it = moveRight(it);

    if (hasElementBeforeLine(it.line_id) == true) {
      byte_between_2_cursor += sizeChar_U8(getCharForLineIdentifier(it.line_id));
    }
    else {
      assert(tmp.file_id.absolute_row != it.file_id.absolute_row);
      byte_between_2_cursor++; // Add one byte alloc to save '\n' char.
    }
  }

  assert(cursor_eq(it, cur2));

  return byte_between_2_cursor;
}

void setSelectCursorOn(Cursor cursor, Cursor* select_cursor) {
  if (cursor_is_disabled(*select_cursor) == true) {
    *select_cursor = cursor;
  }
}

void setSelectCursorOff(Cursor* cursor, Cursor* select_cursor, SELECT_OFF_STYLE style) {
  if (cursor_is_disabled(*select_cursor) == true) {
    return;
  }


  if (style == SELECT_OFF_RIGHT && cursor_le(*cursor, *select_cursor)) {
    Cursor tmp = *cursor;
    *cursor = *select_cursor;
    *select_cursor = tmp;
  }
  else if (style == SELECT_OFF_LEFT && cursor_le(*select_cursor, *cursor)) {
    Cursor tmp = *cursor;
    *cursor = *select_cursor;
    *select_cursor = tmp;
  }

  *select_cursor = cursor_disable(*select_cursor);
}


void selectWord(Cursor* cursor, Cursor* select_cursor) {
  if (cursor_col(*cursor) != 0 && isAWordLetter(getCharForLineIdentifier(cursor->line_id)) == true) {
    *cursor = moveToPreviousWord(*cursor);
  }
  setSelectCursorOn(*cursor, select_cursor);
  *cursor = moveToNextWord(*cursor);
}

void selectLine(Cursor* cursor, Cursor* select_cursor) {
  Cursor tmp = cursorOf(cursor->file_id, moduloLineIdentifierR(getLineForFileIdentifier(cursor->file_id), 0));
  select_cursor->file_id = tryToReachAbsRow(cursor->file_id, cursor_row(*cursor) + 1);
  if (cursor_row(*select_cursor) == cursor_row(*cursor)) {
    tmp = moveLeft(tmp);
    select_cursor->line_id = getLastLineNode(getLineForFileIdentifier(cursor->file_id));
  }
  else {
    select_cursor->line_id = moduloLineIdentifierR(getLineForFileIdentifier(select_cursor->file_id), 0);
  }

  *cursor = tmp;
}

void deleteSelection(Cursor* cursor, Cursor* select_cursor) {
  if (cursor_is_disabled(*select_cursor) == true) {
    return;
  }

  if (cursor_le(*select_cursor, *cursor)) {
    Cursor tmp = *select_cursor;
    *select_cursor = *cursor;
    *cursor = tmp;
  }

  assert(cursor_le(*cursor, *select_cursor));

  *cursor = bulkDelete(*cursor, *select_cursor);

  *select_cursor = cursor_disable(*select_cursor);
}

void deleteSelectionWithState(History** history_p, Cursor* cursor, Cursor* select_cursor,
                              PayloadStateChange payload_state_change) {
  saveAction(history_p, createDeleteAction(*cursor, cursor_to_desc(*select_cursor)), globalOnStageChange, cursor,
             (void*)&payload_state_change);
  deleteSelection(cursor, select_cursor);
}


char* dumpSelection(Cursor cur1, Cursor cur2) {
  if (cursor_is_disabled(cur1) || cursor_is_disabled(cur2)) {
    char* dump = malloc(1 * sizeof(char));
    dump[0] = '\0';
    return dump;
  }


  if (cursor_le(cur2, cur1)) {
    Cursor tmp = cur1;
    cur1 = cur2;
    cur2 = tmp;
  }

  char* dump = NULL;

  unsigned int byte_between_2_cursor = byteBetween2Cursor(cur1, cur2);

  if (byte_between_2_cursor == 0) {
    dump = malloc(1 * sizeof(char));
    dump[0] = '\0';
    return dump;
  }

  assert(byte_between_2_cursor != 0);


  dump = malloc(byte_between_2_cursor * sizeof(char) + 1 /*for EOF char*/);

  Cursor it = cur1;
  int index = 0;
  while (cursor_lt(it, cur2)) {
    it = moveRight(it);

    if (hasElementBeforeLine(it.line_id) == true) {
      Char_U8 ch = getCharForLineIdentifier(it.line_id);
      int ch_size = sizeChar_U8(ch);
      for (int i = 0; i < ch_size; i++) {
        dump[index] = ch.t[i];
        index++;
      }
    }
    else {
      dump[index] = '\n';
      index++;
    }
  }

  dump[index] = '\0';

  return dump;
}


bool isAfterAWord(Cursor* cursor) {
  if (!hasElementBeforeLine(cursor->line_id)) {
    return false;
  }
  Char_U8 u8 = getCharAtCursor(*cursor);
  if (isAWordLetter(u8)) {
    return true;
  }
  if (u8.t[0] == '.') {
    return true;
  }
  return false;
}

void initTextBuffer(TextBuffer* tb, LF_LanguageFeature* feature) {
  tb->cursor = initNewWrittableFile();
  tb->root = tb->cursor.file_id.file;
  tb->select_cursor = cursor_disable(tb->cursor);
  tb->desired_column = 0;

  tb->history_root = malloc(sizeof(History));
  initHistory(tb->history_root);
  tb->history_frame = tb->history_root;

  tb->feature = feature;
}

void destroyTextBuffer(TextBuffer* tb) {
  destroyFullFile(tb->root);
  destroyEndOfHistory(tb->history_root);
  free(tb->history_root);
}

bool tb_handleKey(TextBuffer* tb, int key, PayloadStateChange* state_change) {
  CursorDescriptor tmp_desc;

  switch (key) {
    case H_KEY_LEFT:
      if (cursor_is_disabled(tb->select_cursor)) {
        tb->cursor = moveLeft(tb->cursor);
      }
      setSelectCursorOff(&tb->cursor, &tb->select_cursor, SELECT_OFF_LEFT);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;

    case H_KEY_RIGHT:
      if (cursor_is_disabled(tb->select_cursor)) {
        tb->cursor = moveRight(tb->cursor);
      }
      setSelectCursorOff(&tb->cursor, &tb->select_cursor, SELECT_OFF_RIGHT);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;

    case H_KEY_UP:
      if (cursor_is_disabled(tb->select_cursor)) {
        tb->cursor = moveUp(tb->cursor, tb->desired_column);
      }
      setSelectCursorOff(&tb->cursor, &tb->select_cursor, SELECT_OFF_LEFT);
      return true;

    case H_KEY_DOWN:
      if (cursor_is_disabled(tb->select_cursor)) {
        tb->cursor = moveDown(tb->cursor, tb->desired_column);
      }
      setSelectCursorOff(&tb->cursor, &tb->select_cursor, SELECT_OFF_RIGHT);
      return true;

    case H_KEY_MAJ_LEFT:
      setSelectCursorOn(tb->cursor, &tb->select_cursor);
      tb->cursor = moveLeft(tb->cursor);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;

    case H_KEY_MAJ_RIGHT:
      setSelectCursorOn(tb->cursor, &tb->select_cursor);
      tb->cursor = moveRight(tb->cursor);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;

    case H_KEY_MAJ_UP:
      setSelectCursorOn(tb->cursor, &tb->select_cursor);
      tb->cursor = moveUp(tb->cursor, tb->desired_column);
      return true;

    case H_KEY_MAJ_DOWN:
      setSelectCursorOn(tb->cursor, &tb->select_cursor);
      tb->cursor = moveDown(tb->cursor, tb->desired_column);
      return true;

    case H_KEY_CTRL_LEFT:
      if (cursor_is_disabled(tb->select_cursor)) {
        tb->cursor = moveToPreviousWord(tb->cursor);
      }
      setSelectCursorOff(&tb->cursor, &tb->select_cursor, SELECT_OFF_LEFT);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;

    case H_KEY_CTRL_RIGHT:
      if (cursor_is_disabled(tb->select_cursor)) {
        tb->cursor = moveToNextWord(tb->cursor);
      }
      setSelectCursorOff(&tb->cursor, &tb->select_cursor, SELECT_OFF_RIGHT);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;

    case H_KEY_CTRL_MAJ_LEFT:
      setSelectCursorOn(tb->cursor, &tb->select_cursor);
      tb->cursor = moveToPreviousWord(tb->cursor);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;

    case H_KEY_CTRL_MAJ_RIGHT:
      setSelectCursorOn(tb->cursor, &tb->select_cursor);
      tb->cursor = moveToNextWord(tb->cursor);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;

    case H_KEY_BEGIN:
      tb->select_cursor = cursor_disable(tb->select_cursor);
      tb->cursor = goToBegin(tb->cursor);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;

    case H_KEY_END:
      tb->select_cursor = cursor_disable(tb->select_cursor);
      tb->cursor = goToEnd(tb->cursor);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;

    case H_KEY_MAJ_END:
      setSelectCursorOn(tb->cursor, &tb->select_cursor);
      tb->cursor = goToEnd(tb->cursor);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;

    case K_SPECIAL(K_MOD_CTRL, 'a'):
      tb->select_cursor = tryToReachAbsPosition(tb->cursor, 1, 0);
      tb->cursor = tryToReachAbsPosition(tb->cursor, INT_MAX, INT_MAX);
      return true;

    case K_SPECIAL(K_MOD_CTRL, 'z'):
      tb->select_cursor = cursor_disable(tb->select_cursor);
      tb->cursor =
        undo(&tb->history_frame, tb->cursor, globalOnStageChange, (void*)state_change, &tb->feature->tabulation);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;

    case K_SPECIAL(K_MOD_CTRL, 'y'):
      tb->select_cursor = cursor_disable(tb->select_cursor);
      tb->cursor =
        redo(&tb->history_frame, tb->cursor, globalOnStageChange, (void*)state_change, &tb->feature->tabulation);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;

    case K_SPECIAL(K_MOD_CTRL, 'c'):
      saveToClipBoard(tb->cursor, tb->select_cursor);
      return true;

    case K_SPECIAL(K_MOD_CTRL, 'x'):
      saveToClipBoard(tb->cursor, tb->select_cursor);
      deleteSelectionWithState(&tb->history_frame, &tb->cursor, &tb->select_cursor, *state_change);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;

    case K_SPECIAL(K_MOD_CTRL, 'v'):
      deleteSelectionWithState(&tb->history_frame, &tb->cursor, &tb->select_cursor, *state_change);
      tmp_desc = cursor_to_desc(tb->cursor);
      {
        char paste_buf[4096];
        int len = getClipboardText(paste_buf, sizeof(paste_buf));
        if (len > 0) {
          tb->cursor = insertCharArrayAtCursorWithState(&tb->history_frame, tb->cursor, paste_buf, *state_change,
                                                        &tb->feature->tabulation);
          saveAction(&tb->history_frame, createInsertAction(tb->cursor, tmp_desc), globalOnStageChange, &tb->cursor,
                     (void*)state_change);
        }
      }
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;

    case H_KEY_BACKSPACE: // same as H_KEY_DELETE
      if (cursor_is_disabled(tb->select_cursor)) {
        tb->select_cursor = moveLeft(tb->cursor);
      }
      deleteSelectionWithState(&tb->history_frame, &tb->cursor, &tb->select_cursor, *state_change);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;

    case H_KEY_SUPPR:
      if (cursor_is_disabled(tb->select_cursor)) {
        tb->select_cursor = moveRight(tb->cursor);
      }
      deleteSelectionWithState(&tb->history_frame, &tb->cursor, &tb->select_cursor, *state_change);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;

    case H_KEY_CTRL_DELETE:
    case K_SPECIAL(K_MOD_CTRL, 'h'):
      setSelectCursorOn(tb->cursor, &tb->select_cursor);
      tb->cursor = moveToPreviousWord(tb->cursor);
      deleteSelectionWithState(&tb->history_frame, &tb->cursor, &tb->select_cursor, *state_change);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;

    case H_KEY_CTRL_SUPPR:
      setSelectCursorOn(tb->cursor, &tb->select_cursor);
      tb->cursor = moveToNextWord(tb->cursor);
      deleteSelectionWithState(&tb->history_frame, &tb->cursor, &tb->select_cursor, *state_change);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;
  }

  // Printable UTF-8 codepoints
  if (!K_IS_SPECIAL(key)) {
    int codepoint = K_CODE(key);
    if (codepoint != ERR && (codepoint >= 32 || codepoint == '\t' || codepoint == '\n') && codepoint != 127) {
      deleteSelectionWithState(&tb->history_frame, &tb->cursor, &tb->select_cursor, *state_change);
      tmp_desc = cursor_to_desc(tb->cursor);
      Char_U8 u8 = unicode_to_utf8(codepoint);
      tb->cursor = insertCharInLineC(tb->cursor, u8);
      saveAction(&tb->history_frame, createInsertAction(tb->cursor, tmp_desc), globalOnStageChange, &tb->cursor,
                 (void*)state_change);
      setDesiredColumn(tb->cursor, &tb->desired_column);
      return true;
    }
  }

  return false;
}
