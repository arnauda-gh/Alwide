#include "file_structure.h"
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../utils/tools.h"


////// --------------------LOCAL FUNCTIONS-----------------------------


FileIdentifier insertEmptyLineInFile(FileIdentifier file_id);

FileIdentifier removeLineInFile(FileIdentifier file_id);


////// --------------------UTILS-----------------------------


void memcpy_CharU8Array(Char_U8* dest, const Char_U8* src, int length) { memcpy(dest, src, length * sizeof(Char_U8)); }

void memcpy_LineNodeArray(LineNode* dest, const LineNode* src, int length) {
  memcpy(dest, src, length * sizeof(LineNode));
}

void memcpy_FileNodeArray(FileNode* dest, const FileNode* src, int length) {
  memcpy(dest, src, length * sizeof(FileNode));
}

void memcpy_IntArray(int* dest, const int* src, int length) { memcpy(dest, src, length * sizeof(int)); }

void memmove_CharU8Array(Char_U8* dest, const Char_U8* src, int length) {
  memmove(dest, src, length * sizeof(Char_U8));
}

void memmove_LineNodeArray(LineNode* dest, const LineNode* src, int length) {
  memmove(dest, src, length * sizeof(LineNode));
}

void memmove_FileNodeArray(FileNode* dest, const FileNode* src, int length) {
  memmove(dest, src, length * sizeof(FileNode));
}

void memmove_IntArray(int* dest, const int* src, int length) { memmove(dest, src, length * sizeof(int)); }

Char_U8* malloc_CharU8Array(int length) { return malloc(length * sizeof(Char_U8)); }

LineNode* malloc_LineNodeArray(int length) { return malloc(length * sizeof(LineNode)); }

FileNode* malloc_FileNodeArray(int length) { return malloc(length * sizeof(FileNode)); }

int* malloc_IntArray(int length) { return malloc(length * sizeof(int)); }

Char_U8* realloc_CharU8Array(Char_U8* array, int length) { return realloc(array, length * sizeof(Char_U8)); }

LineNode* realloc_LineNodeArray(LineNode* array, int length) { return realloc(array, length * sizeof(LineNode)); }

FileNode* realloc_FileNodeArray(FileNode* array, int length) { return realloc(array, length * sizeof(FileNode)); }

int* realloc_IntArray(int* array, int length) { return realloc(array, length * sizeof(int)); }

////// --------------------LINE-----------------------------

int byteCountForLineNode(LineNode* line, int index_start, int length) {
  int count = 0;
  for (int i = index_start; i < index_start + length; i++) {
    count += utf8_size(line->ch[i]);
  }
  return count;
}

int byteCountForCurrentLineToEnd(LineNode* line, int index_start) {
  int count = 0;
  if (index_start == 0) {
    count += line->byte_count;
  }
  else {
    count += byteCountForLineNode(line, index_start, line->element_number - index_start);
  }

  line = line->next;
  while (line != NULL) {
    count += line->byte_count;
    line = line->next;
  }

  return count;
}

/**
 *  Init a new empty head of LineNode.
 */
void initEmptyLineNode(LineNode* line) {
  assert(line != NULL);
  line->fixed = false;
  line->next = NULL;
  line->prev = NULL;
  line->ch = NULL;
  line->current_max_element_number = 0;
  line->element_number = 0;
  line->byte_count = 0;
}


/**
 *  Init a new head of LineNode.
 */
void initLineNode(LineNode* line, Size size);

/**
 *  Return the number of char in the LineNode.
 */
int sizeLineNode(LineNode* line) {
  int count = 0;
  while (line != NULL) {
    count += line->element_number;
    line = line->next;
  }
  return count;
}

/**
 * Insert an EMPTY node after the node given.
 */
LineNode* insertLineNodeBefore(LineNode* node) {
  LineNode* newNode = malloc_LineNodeArray(1);
  initEmptyLineNode(newNode);

  newNode->prev = node->prev;
  node->prev = newNode;
  if (newNode->prev != NULL) {
    newNode->prev->next = newNode;
  }
  newNode->next = node;

  return newNode;
}


/**
 * Insert an EMPTY node after the node given.
 */
LineNode* insertLineNodeAfter(LineNode* node) {
  LineNode* newNode = malloc_LineNodeArray(1);
  initEmptyLineNode(newNode);

  newNode->next = node->next;
  newNode->prev = node;
  if (newNode->next != NULL) {
    newNode->next->prev = newNode;
  }
  node->next = newNode;

  return newNode;
}

/**
 * Free the current Node. And return the prev node if not NULL or the next node if not NULL.
 */
LineNode* destroyCurrentLineNode(LineNode* node) {
  LineNode* newNode = NULL;

  if (node->prev != NULL) {
    newNode = node->prev;
  }
  else if (node->next != NULL) {
    newNode = node->next;
  }


  if (node->next != NULL) {
    node->next->prev = node->prev;
  }

  if (node->prev != NULL) {
    node->prev->next = node->next;
  }

  assert(node->prev == NULL || node->prev->next == node->next);
  assert(node->next == NULL || node->next->prev == node->prev);

  free(node->ch);
  node->byte_count = 0;
  assert(node->fixed == false);
  free(node);
  return newNode;
}


/**
 * -1     => failed.
 * 0      => access to index 0 from the next node
 * other  => Number of Char_U8 moved to next node.
 */
int slideFromLineNodeToNextLineNodeAfterIndex(LineNode* node, int index) {
  if (node->next == NULL) {
    return -1;
  }

  if (node->next->element_number == MAX_ELEMENT_NODE) {
    return -1;
  }

  if (node->next->current_max_element_number != MAX_ELEMENT_NODE &&
      node->element_number - index + 1 > node->next->current_max_element_number - node->next->element_number) {
    // If previous is not already full allocated && need more space to store
    node->next->current_max_element_number =
      min(node->next->current_max_element_number + node->element_number - index + 1 + CACHE_SIZE, MAX_ELEMENT_NODE);
    assert(node->next->current_max_element_number <= MAX_ELEMENT_NODE);
    node->next->ch = realloc_CharU8Array(node->next->ch, node->next->current_max_element_number);
  }

  if (index == MAX_ELEMENT_NODE) {
    return 0;
  }

  int moved = min(node->element_number - index, node->next->current_max_element_number - node->next->element_number);
#ifdef LOGS
  fprintf(stderr, "SLIDING RIGHT NUMBER %d\r\n", moved);
#endif
  assert(node->next->element_number + moved <= MAX_ELEMENT_NODE);


  int count = byteCountForLineNode(node, node->element_number - moved, moved);

  memmove_CharU8Array(node->next->ch + moved, node->next->ch, node->next->element_number);
  memcpy_CharU8Array(node->next->ch, node->ch + node->element_number - moved, moved);

  node->byte_count -= count;
  node->next->byte_count += count;

  node->element_number -= moved;
  node->next->element_number += moved;
  return moved;
}


/**
 * -1     => failed.
 * 0      => access to last item from the previous node
 * other  => Number of Char_U8 moved to previous node.
 */
int slideFromLineNodeToPreviousLineNodeBeforeIndex(LineNode* node, int index) {
  if (node->prev == NULL) {
    return -1;
  }

  if (node->prev->element_number == MAX_ELEMENT_NODE) {
    return -1;
  }


  if (node->prev->current_max_element_number != MAX_ELEMENT_NODE &&
      index + 1 > node->prev->current_max_element_number - node->prev->element_number) {
    // If previous is not already full allocated && need more space to store
#ifdef LOGS
    fprintf(stderr, "Realloc previous ");
#endif
    node->prev->current_max_element_number =
      min(node->prev->current_max_element_number + index + 1 + CACHE_SIZE, MAX_ELEMENT_NODE);
    assert(node->prev->current_max_element_number <= MAX_ELEMENT_NODE);
#ifdef LOGS
    fprintf(stderr, " new size %d\n\r", node->prev->current_max_element_number);
#endif
    node->prev->ch = realloc_CharU8Array(node->prev->ch, node->prev->current_max_element_number);
  }
#ifdef LOGS
  fprintf(stderr, "Min Of Index : %d and %d - %d  = %d  \r\n", index, node->prev->current_max_element_number,
          node->prev->element_number, node->prev->current_max_element_number - node->prev->element_number);
#endif

  if (index == 0) {
    return 0;
  }

  int moved = min(index, node->prev->current_max_element_number - node->prev->element_number);
  // fprintf(stderr, "Moved :
  assert(node->prev->element_number + moved <= MAX_ELEMENT_NODE);


  int count = byteCountForLineNode(node, 0, moved);

  memcpy_CharU8Array(node->prev->ch + node->prev->element_number, node->ch, moved);
  memmove_CharU8Array(node->ch, node->ch + moved, node->element_number - moved);

  node->byte_count -= count;
  node->prev->byte_count += count;

  node->element_number -= moved;
  node->prev->element_number += moved;
  return moved;
}


/**
 * Get 1 case open in line
 * Return the new relative index for line cell.
 * Shift cannot be positive.
 *
 * If index == 0 focus on previous.
 *
 * If the current node has place focus on it.
 *
 * Else focus on previous.
 *
 * Else focus on next.
 *
 *
 */
int allocateOneCharAtIndex(LineNode* line, int index) {
  assert(index >= 0);
  assert(index <= MAX_ELEMENT_NODE);

  int available_here = MAX_ELEMENT_NODE - line->element_number;

  if (index != 0 || line->prev == NULL || line->prev->element_number == MAX_ELEMENT_NODE) {
    if (available_here >= 1) {
      // Is size available in current cell
#ifdef LOGS
      fprintf(stderr, "Simple case !\n\r");
#endif

      if (line->current_max_element_number - line->element_number < 1) {
#ifdef LOGS
        fprintf(stderr, "Realloc mem\n\r");
#endif
        // Need to realloc memory ?
        line->current_max_element_number = min(line->current_max_element_number + 1 + CACHE_SIZE, MAX_ELEMENT_NODE);
        assert(line->current_max_element_number <= MAX_ELEMENT_NODE);
        line->ch = realloc_CharU8Array(line->ch, line->current_max_element_number);
      }

      assert(line->current_max_element_number - line->element_number >= 1);
      return 0;
    }

    assert(available_here == 0);
  }

#ifdef LOGS
  fprintf(stderr, "Hard case !\n\r");
#endif

  // Current cell cannot contain the asked size.
  int available_prev = line->prev == NULL ? 0 : MAX_ELEMENT_NODE - line->prev->element_number;
  int available_next = line->next == NULL ? 0 : MAX_ELEMENT_NODE - line->next->element_number;
#ifdef LOGS
  fprintf(stderr, "AVAILABLE PREVIOUS %d | NEXT %d | CURRENT %d \r\n", available_prev, available_next, available_here);
#endif
  if (available_prev + available_next + available_here < 1) {
    // Need to insert node
#ifdef LOGS
    fprintf(stderr, "INSERTING A NODE\r\n");
#endif

    if (index == 0 && !line->fixed) {
      // Due to the fact that we can just deallocate instant from right of an array only more power full for index == 0.
      insertLineNodeBefore(line);
      available_prev = MAX_ELEMENT_NODE;
    }
    else {
      insertLineNodeAfter(line);
      available_next = MAX_ELEMENT_NODE;
    }
  }

  assert(available_prev + available_next >= 1);

  int shift = 0;
  if (available_prev != 0 && !line->fixed) {
#ifdef LOGS
    fprintf(stderr, "SLIDING LEFT\r\n");
#endif
    int moved = slideFromLineNodeToPreviousLineNodeBeforeIndex(line, index);
#ifdef LOGS
    fprintf(stderr, "Moved : %d Shift %d\r\n", moved, shift);
#endif
    assert(moved != -1);
    shift -= moved;
  }
  else if (MAX_ELEMENT_NODE - line->element_number < 1) {
    // assert(index != MAX_ELEMENT_NODE);
#ifdef LOGS
    fprintf(stderr, "SLIDING RIGHT\r\n");
#endif
    slideFromLineNodeToNextLineNodeAfterIndex(line, index);
  }
#ifdef LOGS
  fprintf(stderr, "When return %d\n\r", shift);
#endif
  return shift;
}

/**
 * Return idenfier for the node containing current relative index.
 * Return an index as 0 < index <= line->element_number
 * Index == 0 is possible if index = 0 and previous node doesn't exist.
 */
LineIdentifier moduloLineIdentifierR(LineNode* line, int column) {
  int abs_column = column;
  assert(line != NULL);
  while (column < 0 || (column == 0 && line->prev != NULL)) {
    assert(line->prev != NULL); // Index out of range
    column += line->prev->element_number;
    line = line->prev;
  }

  while (column > line->element_number) {
    column -= line->element_number;
    assert(line->next != NULL); // Index out of range
    line = line->next;
  }

  LineIdentifier line_id;
  line_id.line = line;
  line_id.relative_column = column;
  line_id.absolute_column = abs_column;

  return line_id;
}

LineIdentifier moduloLineIdentifier(LineIdentifier line_id) {
  LineIdentifier tmp_line_id = moduloLineIdentifierR(line_id.line, line_id.relative_column);
  tmp_line_id.absolute_column = line_id.absolute_column;
  return tmp_line_id;
}

/**
 * Insert a char at index of the line node.
 * Return the new relative index for line cell.
 */
LineIdentifier insertCharInLine(LineIdentifier line_id, Char_U8 ch) {
  line_id = moduloLineIdentifier(line_id);

  LineNode* line = line_id.line;
  int column = line_id.relative_column;

  int relative_shift = allocateOneCharAtIndex(line, column);
#ifdef LOGS
  fprintf(stderr, "Shift : %d & Index : %d\n\r", relative_shift, column);
#endif
  column = column + relative_shift;

  assert(relative_shift <= 0);
  assert(column <= MAX_ELEMENT_NODE);
  assert(column >= 0);

  if (column == 0) {
    if (line->prev != NULL && line->prev->element_number != MAX_ELEMENT_NODE) {
      // can add previous.
#ifdef LOGS
      fprintf(stderr, "SHIFT PREVIOUS DETECTED\n\r");
#endif
      assert(line->prev != NULL);
      line = line->prev;
      column = line->element_number;
    }
    else {
      // cannot add previous assert place here.
      assert(line->element_number != MAX_ELEMENT_NODE);
    }
  }
  else if (column == MAX_ELEMENT_NODE) {
    assert(line->next != NULL);
#ifdef LOGS
    fprintf(stderr, "SHIFT NEXT DETECTED\n\r");
#endif
    line = line->next;
    column = 0;
  }

  if (column == line->element_number) {
    // Simple happend
    line->ch[column] = ch;
    line->element_number++;
    line->byte_count += utf8_size(ch);
#ifdef LOGS
    fprintf(stderr, "ADD AT THE END\r\n");
#endif
  }
  else {
    // Insert in array
#ifdef LOGS
    fprintf(stderr, "INSERT IN MIDDLE\r\n");
#endif
    assert(line->element_number - column > 0);
    memmove_CharU8Array(line->ch + column + 1, line->ch + column, line->element_number - column);
    line->ch[column] = ch;
    line->element_number++;
    line->byte_count += utf8_size(ch);
  }

  line_id.line = line;
  line_id.relative_column = column + 1;
  line_id.absolute_column++;

  return line_id;
}


/**
 * Insert a char at index of the line node.
 */
LineIdentifier removeCharInLine(LineIdentifier line_id) {
  line_id = moduloLineIdentifier(line_id);
  line_id.relative_column--;
  LineNode* line = line_id.line;
  int cursorPos = line_id.relative_column;

  if (cursorPos == -1) {
    assert(line->prev != NULL);
    line = line->prev;
    cursorPos = line->element_number;
    line_id.line = line;
    line_id.relative_column = cursorPos;
  }

#ifdef LOGS
  fprintf(stderr, "REMOVE CHAR RELATIVE %d \r\n", cursorPos);
  fprintf(stderr, "CURRENT LINE ELEMENT NUMBER %d\r\n", line->element_number);
#endif
  assert(cursorPos >= 0);
  assert(cursorPos < line->element_number);

  if (cursorPos != line->element_number - 1) {
#ifdef LOGS
    fprintf(stderr, "REMOVE IN MIDDLE. Table Size %d. Element Number %d\r\n", line->current_max_element_number,
            line->element_number);
#endif
    // fprintf(stderr, "At move : %d, first %d, ")
    line->byte_count -= utf8_size(line->ch[cursorPos]);
    memmove_CharU8Array(line->ch + cursorPos, line->ch + cursorPos + 1, line->element_number - cursorPos - 1);
  }
  else {
#ifdef LOGS
    fprintf(stderr, "REMOVE ATT END\r\n");
#endif
  }

  line->element_number--;

  if (line->element_number == 0) {
    // the current node is empty.
    int available_prev = line->prev == NULL ? 0 : MAX_ELEMENT_NODE - line->prev->element_number;
    int available_next = line->next == NULL ? 0 : MAX_ELEMENT_NODE - line->next->element_number;

    if (available_prev != 0 || available_next != 0) {
      assert(line->prev != NULL || line->next != NULL);
#ifdef LOGS
      fprintf(stderr, "FREE NODE\r\n");
#endif
      if (line->next != NULL && line->next->element_number == 0) {
        assert(line->next->fixed == false);
        destroyCurrentLineNode(line->next);
      }
      else {
        if (line->fixed == true) {
          assert(line->next->fixed == false);
          slideFromLineNodeToPreviousLineNodeBeforeIndex(line->next, line->next->element_number);
          assert(line->next->element_number == 0);
          destroyCurrentLineNode(line->next);
          line_id.line = line;
          line_id.relative_column = 0;
        }
        else {
          line = destroyCurrentLineNode(line);
          line_id.line = line;
          line_id.relative_column = line->element_number;
        }
      }
    }
    else {
      if (line->current_max_element_number > CACHE_SIZE + 1) {
#ifdef LOGS
        fprintf(stderr, "REALLOC ONLY CACHE\r\n");
#endif
        line->current_max_element_number = min(MAX_ELEMENT_NODE, CACHE_SIZE);
        assert(line->current_max_element_number <= MAX_ELEMENT_NODE);
        line->ch = realloc_CharU8Array(line->ch, line->current_max_element_number);
      }
    }
  }

#ifdef LOGS
  fprintf(stderr, "REMOVE ENDED\r\n");
#endif
  line_id.absolute_column--;
  return line_id;
}


Char_U8 getCharForLineIdentifier(LineIdentifier line_id) {
  line_id = moduloLineIdentifier(line_id);
  if (line_id.relative_column == 0) {
    assert(line_id.line->prev != NULL); // OUT OF RANGE.
    return line_id.line->prev->ch[line_id.line->prev->element_number - 1];
  }
  return line_id.line->ch[line_id.relative_column - 1];
}


LineIdentifier getLastLineNode(LineNode* line) {
  assert(line != NULL);
  int abs_column = 0;
  while (line->next != NULL) {
    abs_column += line->element_number;
    line = line->next;
  }

  LineIdentifier line_id;
  line_id.relative_column = line->element_number;
  line_id.line = line;
  line_id.absolute_column = abs_column + line->element_number;

  return line_id;
}

int getAbsoluteLineIndex(LineIdentifier id) {
  assert(id.line != NULL);

  int abs = id.relative_column;
  id.line = id.line->prev;

  while (id.line != NULL) {
    abs += id.line->element_number;
    id.line = id.line->prev;
  }

  return abs;
}


bool isEmptyLine(LineNode* line) {
  while (line != NULL) {
    if (line->element_number != 0) {
      return false;
    }
    line = line->next;
  }
  return true;
}

bool hasElementAfterLine(LineIdentifier line_id) {
  return line_id.relative_column != line_id.line->element_number || isEmptyLine(line_id.line->next) == false;
}

bool hasElementBeforeLine(LineIdentifier line_id) { return line_id.absolute_column != 0; }


void printLineNode(LineNode* line) {
  for (int i = 0; i < line->element_number; i++) {
    printChar_U8(stdout, line->ch[i]);
  }
}

LineIdentifier tryToReachAbsColumn(LineIdentifier line_id, int abs_column) {
  if (line_id.absolute_column == abs_column) {
    return line_id;
  }

  line_id = moduloLineIdentifier(line_id);

  line_id.absolute_column -= line_id.relative_column;
  line_id.relative_column = 0;

  bool first = false;

  while (line_id.absolute_column + line_id.line->element_number < abs_column) {
    first = true;
    if (line_id.line->next == NULL) {
      break;
    }
    line_id.absolute_column += line_id.line->element_number - line_id.relative_column;
    line_id.line = line_id.line->next;
    line_id.relative_column = 0;
  }

  while (abs_column < line_id.absolute_column) {
    assert(first == false);
    assert(line_id.relative_column == 0);
    if (line_id.line->prev == NULL) {
      break;
    }
    line_id.absolute_column -= line_id.line->prev->element_number;
    line_id.line = line_id.line->prev;
    line_id.relative_column = 0; // no needed.
  }

  if (line_id.absolute_column < abs_column) {
    int toAdd = min(line_id.line->element_number - line_id.relative_column, abs_column - line_id.absolute_column);
    line_id.absolute_column += toAdd;
    line_id.relative_column += toAdd;
    assert(line_id.relative_column == line_id.line->element_number || abs_column == line_id.absolute_column);
  }

  return moduloLineIdentifier(line_id);
}


void deleteLinePart(LineIdentifier line_id, int length) {
  line_id = moduloLineIdentifier(line_id);

  int current_removed = 0;
  while (current_removed < length) {
    // If it's the line end go to next.
    if (line_id.relative_column == line_id.line->element_number) {
      line_id.line = line_id.line->next;
      assert(line_id.line != NULL);
      line_id.relative_column = 0;
    }

    // If the id is the begin of a node and If the node can be completely removed. We can improve perf.
    if (line_id.relative_column == 0 && line_id.line->element_number <= length - current_removed) {
      current_removed += line_id.line->element_number;
      if (line_id.line->fixed == false) {
        // If the node can be cleared.
        line_id.line = destroyCurrentLineNode(line_id.line);
        line_id.relative_column = line_id.line->element_number;
      }
      else {
        // If the node is fixed.
        // assert(false);
        line_id.line->element_number = 0;
        line_id.line->current_max_element_number = 0;
        free(line_id.line->ch);
        line_id.line->ch = NULL;
        line_id.line->byte_count = 0;
      }
    }
    else {
      // Need to delete a part of the node.
      int atDelete = min(length - current_removed, line_id.line->element_number - line_id.relative_column);
      int count_byte_removed = byteCountForLineNode(line_id.line, line_id.relative_column, atDelete);
      memmove_CharU8Array(line_id.line->ch + line_id.relative_column,
                          line_id.line->ch + line_id.relative_column + atDelete,
                          line_id.line->element_number - line_id.relative_column - atDelete);
      line_id.line->element_number -= atDelete;
      line_id.line->byte_count -= count_byte_removed;
      current_removed += atDelete;

      // Realloc line.ch if too much unused mem.
      if (line_id.line->current_max_element_number > line_id.line->element_number + CACHE_SIZE) {
        line_id.line->ch = realloc_CharU8Array(line_id.line->ch, line_id.line->element_number + CACHE_SIZE);
        line_id.line->current_max_element_number = line_id.line->element_number + CACHE_SIZE;
        assert(line_id.line->current_max_element_number <= MAX_ELEMENT_NODE);
      }
    }
  }
}


/**
 * Destroy line free all memory.
 * Will check Node before.
 */
void destroyFullLine(LineNode* node) {
  while (node != NULL && node->prev != NULL) {
    node = node->prev;
  }

  while (node != NULL) {
    LineNode* tmp = node;
    node = node->next;

    free(tmp->ch);
    if (tmp->fixed == false) {
      tmp->byte_count = 0;
      free(tmp);
    }
    else {
      initEmptyLineNode(tmp);
      tmp->fixed = true;
    }
  }
}

/**
 *
 * Destroy line letting this current node.
 */
void destroyChildLine(LineNode* node) {
  assert(node != NULL);
  free(node->ch);
  node->ch = NULL;
  node->current_max_element_number = 0;
  node->element_number = 0;
  node = node->next;

  while (node != NULL) {
    LineNode* tmp = node;
    node = node->next;

    free(tmp->ch);
    if (tmp->fixed == false) {
      tmp->byte_count = 0;
      free(tmp);
    }
    else {
      initEmptyLineNode(tmp);
      tmp->fixed = true;
    }
  }
}


///// ----------------------- FILE ---------------------------------

int sumLinesBytes(int* array, int length) {
  int total = 0;
  for (int i = 0; i < length; i++) {
    total += array[i];
  }
  return total;
}

/**
 *  Init a new empty head of FileNode.
 */
void initEmptyFileNode(FileNode* file) {
  assert(file != NULL);
  file->next = NULL;
  file->prev = NULL;
  file->lines = NULL;
  file->current_max_element_number = 0;
  file->element_number = 0;
  file->byte_count = 0;
  for (int i = 0; i < MAX_ELEMENT_NODE; i++) {
    file->lines_byte_count[i] = 0;
  }
}

Cursor initNewWrittableFile() {
  FileNode* file = malloc_FileNodeArray(1);
  initEmptyFileNode(file);

  FileIdentifier file_id = insertEmptyLineInFile(moduloFileIdentifierR(file, 0));
  assert(file_id.file == file);
  assert(file_id.relative_row == 1);

  return moduloCursorR(file, 1, 0);
}

/**
 * length == -1 => mean that length is
 */
void rebindFileNode(FileNode* file, int start, int length) {
  if (length == -1) {
    length = file->element_number - start;
  }
  assert(start + length <= file->element_number);
  assert(start >= 0);
  for (int i = start; i < start + length; i++) {
    if (file->lines[i].next != NULL) {
      file->lines[i].next->prev = file->lines + i;
    }
  }
}

void rebindFullFileNode(FileNode* file) { rebindFileNode(file, 0, -1); }


/**
 * Insert an EMPTY node after the node given.
 */
FileNode* insertFileNodeBefore(FileNode* file) {
  FileNode* newNode = malloc_FileNodeArray(1);
  initEmptyFileNode(newNode);

  newNode->prev = file->prev;
  file->prev = newNode;
  if (newNode->prev != NULL) {
    newNode->prev->next = newNode;
  }
  newNode->next = file;

  return newNode;
}


/**
 * Insert an EMPTY node after the node given.
 */
FileNode* insertFileNodeAfter(FileNode* file) {
  FileNode* newNode = malloc_FileNodeArray(1);
  initEmptyFileNode(newNode);

  newNode->next = file->next;
  newNode->prev = file;
  if (newNode->next != NULL) {
    newNode->next->prev = newNode;
  }
  file->next = newNode;

  return newNode;
}

/**
 * Free the current Node. And return the prev node if not NULL or the next node if not NULL.
 */
FileNode* destroyCurrentFileNode(FileNode* file) {
  FileNode* newNode = NULL;

  if (file->prev != NULL) {
    newNode = file->prev;
  }
  else if (file->next != NULL) {
    newNode = file->next;
  }


  if (file->next != NULL) {
    file->next->prev = file->prev;
  }

  if (file->prev != NULL) {
    file->prev->next = file->next;
  }

  assert(file->prev == NULL || file->prev->next == file->next);
  assert(file->next == NULL || file->next->prev == file->prev);

  for (int i = 0; i < file->element_number; i++) {
    destroyFullLine(file->lines + i);
  }
  free(file->lines);
  free(file);
  return newNode;
}


/**
 * -1     => failed.
 * 0      => access to index 0 from the next node
 * other  => Number of Char_U8 moved to next node.
 */
int slideFromFileNodeToNextFileNodeAfterIndex(FileNode* file, int row) {
  if (file->next == NULL) {
    return -1;
  }

  if (file->next->element_number == MAX_ELEMENT_NODE) {
    return -1;
  }

  if (file->next->current_max_element_number != MAX_ELEMENT_NODE &&
      file->element_number - row + 1 > file->next->current_max_element_number - file->next->element_number) {
    // If previous is not already full allocated && need more space to store
    file->next->current_max_element_number =
      min(file->next->current_max_element_number + file->element_number - row + 1 + CACHE_SIZE, MAX_ELEMENT_NODE);
    assert(file->next->current_max_element_number <= MAX_ELEMENT_NODE);
    LineNode* old_tab = file->next->lines;
    file->next->lines = realloc_LineNodeArray(file->next->lines, file->next->current_max_element_number);
    if (file->next->lines != old_tab) {
      rebindFullFileNode(file->next);
    }
  }

  if (row == MAX_ELEMENT_NODE) {
    return 0;
  }

  int moved = min(file->element_number - row, file->next->current_max_element_number - file->next->element_number);
#ifdef LOGS
  fprintf(stderr, "SLIDING RIGHT NUMBER %d\r\n", moved);
#endif
  assert(file->next->element_number + moved <= MAX_ELEMENT_NODE);


  memmove_IntArray(file->next->lines_byte_count + moved, file->next->lines_byte_count, file->next->element_number);
  memmove_LineNodeArray(file->next->lines + moved, file->next->lines, file->next->element_number);

  int sum = sumLinesBytes(file->lines_byte_count + file->element_number - moved, moved);
  memcpy_IntArray(file->next->lines_byte_count, file->lines_byte_count + file->element_number - moved, moved);
  memcpy_LineNodeArray(file->next->lines, file->lines + file->element_number - moved, moved);
  file->byte_count -= sum + moved;
  file->next->byte_count += sum + moved;


  file->element_number -= moved;
  file->next->element_number += moved;

  rebindFullFileNode(file->next);
  return moved;
}


/**
 * -1     => failed.
 * 0      => access to last item from the previous node
 * other  => Number of Char_U8 moved to previous node.
 */
int slideFromFileNodeToPreviousFileNodeBeforeIndex(FileNode* file, int row) {
  if (file->prev == NULL) {
    return -1;
  }

  if (file->prev->element_number == MAX_ELEMENT_NODE) {
    return -1;
  }


  if (file->prev->current_max_element_number != MAX_ELEMENT_NODE &&
      row + 1 > file->prev->current_max_element_number - file->prev->element_number) {
    // If previous is not already full allocated && need more space to store
#ifdef LOGS
    fprintf(stderr, "Realloc previous ");
#endif
    file->prev->current_max_element_number =
      min(file->prev->current_max_element_number + row + 1 + CACHE_SIZE, MAX_ELEMENT_NODE);
    assert(file->prev->current_max_element_number <= MAX_ELEMENT_NODE);
#ifdef LOGS
    fprintf(stderr, " new size %d\n\r", file->prev->current_max_element_number);
#endif
    LineNode* old_tab = file->prev->lines;
    file->prev->lines = realloc_LineNodeArray(file->prev->lines, file->prev->current_max_element_number);
    if (file->prev->lines != old_tab) {
      rebindFullFileNode(file->prev);
    }
  }
#ifdef LOGS
  fprintf(stderr, "Min Of Index : %d and %d - %d  = %d  \r\n", row, file->prev->current_max_element_number,
          file->prev->element_number, file->prev->current_max_element_number - file->prev->element_number);
#endif

  if (row == 0) {
    return 0;
  }

  int moved = min(row, file->prev->current_max_element_number - file->prev->element_number);
  assert(file->prev->element_number + moved <= MAX_ELEMENT_NODE);

  int sum = sumLinesBytes(file->lines_byte_count, moved);
  memcpy_IntArray(file->prev->lines_byte_count + file->prev->element_number, file->lines_byte_count, moved);
  memcpy_LineNodeArray(file->prev->lines + file->prev->element_number, file->lines, moved);
  file->byte_count -= sum + moved;
  file->prev->byte_count += sum + moved;

  memmove_IntArray(file->lines_byte_count, file->lines_byte_count + moved, file->element_number - moved);
  memmove_LineNodeArray(file->lines, file->lines + moved, file->element_number - moved);


  file->element_number -= moved;
  file->prev->element_number += moved;
  rebindFullFileNode(file);
  rebindFileNode(file->prev, file->prev->element_number - moved, moved);

  return moved;
}


/**
 * Get 1 case open in line
 * Return the new relative index for line cell.
 * Shift cannot be positive.
 *
 * If the current node has place focus on it.
 *
 * Else focus on previous.
 *
 * Else focus on next.
 *
 *
 */
int allocateOneRowInFile(FileNode* file, int row) {
  assert(row >= 0);
  assert(row <= MAX_ELEMENT_NODE);

  int available_here = MAX_ELEMENT_NODE - file->element_number;

  if (available_here >= 1) {
    // Is size available in current cell
#ifdef LOGS
    fprintf(stderr, "Simple case !\n\r");
#endif

    if (file->current_max_element_number - file->element_number < 1) {
#ifdef LOGS
      fprintf(stderr, "Realloc mem\n\r");
#endif
      // Need to realloc memory ?
      file->current_max_element_number = min(file->current_max_element_number + 1 + CACHE_SIZE, MAX_ELEMENT_NODE);
      assert(file->current_max_element_number <= MAX_ELEMENT_NODE);
      LineNode* old_tab = file->lines;
      file->lines = realloc_LineNodeArray(file->lines, file->current_max_element_number);
      if (file->lines != old_tab) {
        rebindFullFileNode(file);
      }
    }

    assert(file->current_max_element_number - file->element_number >= 1);
    return 0;
  }

  assert(available_here == 0);
#ifdef LOGS
  fprintf(stderr, "Hard case !\n\r");
#endif

  // Current cell cannot contain the asked size.
  int available_prev = file->prev == NULL ? 0 : MAX_ELEMENT_NODE - file->prev->element_number;
  int available_next = file->next == NULL ? 0 : MAX_ELEMENT_NODE - file->next->element_number;
#ifdef LOGS
  fprintf(stderr, "AVAILABLE PREVIOUS %d | NEXT %d | CURRENT %d \r\n", available_prev, available_next, available_here);
#endif

  if (available_prev + available_next + available_here < 1) {
    // Need to insert node
#ifdef LOGS
    fprintf(stderr, "INSERTING A NODE\r\n");
#endif

    if (row == 0 && file->prev != NULL /*AVOID move previous for root FileNode*/) {
      // Due to the fact that we can just deallocate instant from right of an array only more power full for index == 0.
      insertFileNodeBefore(file);
      available_prev = MAX_ELEMENT_NODE;
    }
    else {
      insertFileNodeAfter(file);
      available_next = MAX_ELEMENT_NODE;
    }
  }

  assert(available_prev + available_next >= 1);

  int shift = 0;
  if (available_prev != 0) {
#ifdef LOGS
    fprintf(stderr, "SLIDING LEFT\r\n");
#endif
    int moved = slideFromFileNodeToPreviousFileNodeBeforeIndex(file, row);
#ifdef LOGS
    fprintf(stderr, "Moved : %d Shift %d\r\n", moved, shift);
#endif
    assert(moved != -1);
    shift -= moved;
  }
  else if (MAX_ELEMENT_NODE - file->element_number < 1) {
    // assert(index != MAX_ELEMENT_NODE);
#ifdef LOGS
    fprintf(stderr, "SLIDING RIGHT\r\n");
#endif
    slideFromFileNodeToNextFileNodeAfterIndex(file, row);
  }
#ifdef LOGS
  fprintf(stderr, "When return %d\n\r", shift);
#endif
  return shift;
}

/**
 * Return idenfier for the node containing current relative index.
 * Return an index as 0 <= index <= line->element_number
 */
FileIdentifier moduloFileIdentifierR(FileNode* file, int row) {
  int abs_row = row;
  while (row < 0 || (row == 0 && file->prev != NULL)) {
    assert(file->prev != NULL); // Index out of range
    row += file->prev->element_number;
    file = file->prev;
  }

  while (row > file->element_number) {
    row -= file->element_number;
    assert(file->next != NULL); // Index out of range
    file = file->next;
  }

  FileIdentifier line_id;
  line_id.file = file;
  line_id.relative_row = row;
  line_id.absolute_row = abs_row;

  return line_id;
}

FileIdentifier moduloFileIdentifier(FileIdentifier file_id) {
  FileIdentifier tmp_file_id = moduloFileIdentifierR(file_id.file, file_id.relative_row);
  tmp_file_id.absolute_row = file_id.absolute_row;
  return tmp_file_id;
}


/**
 * Insert a char at index of the line node.
 * Return the new relative index for line cell.
 */
FileIdentifier insertEmptyLineInFile(FileIdentifier file_id) {
  file_id = moduloFileIdentifier(file_id);

  FileNode* file = file_id.file;
  int row = file_id.relative_row;

  int relative_shift = allocateOneRowInFile(file, row);

#ifdef LOGS
  fprintf(stderr, "Shift : %d & Index : %d\n\r", relative_shift, row);
#endif
  row = row + relative_shift;

  assert(relative_shift <= 0);
  assert(row <= MAX_ELEMENT_NODE);
  assert(row >= 0);

  if (row == 0) {
    if (file->prev != NULL && file->prev->element_number != MAX_ELEMENT_NODE) {
      // can add previous.
#ifdef LOGS
      fprintf(stderr, "SHIFT PREVIOUS DETECTED\n\r");
#endif
      assert(file->prev != NULL);
      file = file->prev;
      row = file->element_number;
    }
    else {
      // cannot add previous assert place here.
      assert(file->element_number != MAX_ELEMENT_NODE);
    }
  }
  else if (row == MAX_ELEMENT_NODE) {
    assert(file->next != NULL);
#ifdef LOGS
    fprintf(stderr, "SHIFT NEXT DETECTED\n\r");
#endif
    file = file->next;
    row = 0;
  }

  if (row == file->element_number) {
    // Simple happend
    initEmptyLineNode(file->lines + row);
    file->lines[row].fixed = true;
    file->element_number++;
    file->byte_count += 1;
    file->lines_byte_count[row] = 0;

#ifdef LOGS
    fprintf(stderr, "ADD AT THE END\r\n");
#endif
  }
  else {
    // Insert in array
#ifdef LOGS
    fprintf(stderr, "INSERT IN MIDDLE\r\n");
#endif
    assert(file->element_number - row > 0);
    memmove_IntArray(file->lines_byte_count + row + 1, file->lines_byte_count + row, file->element_number - row);
    memmove_LineNodeArray(file->lines + row + 1, file->lines + row, file->element_number - row);
    initEmptyLineNode(file->lines + row);
    file->lines[row].fixed = true;
    file->element_number++;
    file->byte_count += 1;
    file->lines_byte_count[row] = 0;
    rebindFileNode(file, row + 1, -1);
  }

  file_id.file = file;
  file_id.relative_row = row + 1;
  file_id.absolute_row++;

  return moduloFileIdentifier(file_id);
}


/**
 * Remove a line at index of the line node.
 */
FileIdentifier removeLineInFile(FileIdentifier file_id) {
  file_id = moduloFileIdentifier(file_id);
  file_id.relative_row--;
  file_id.absolute_row--;
  FileNode* file = file_id.file;
  int row = file_id.relative_row;

  if (row == -1) {
    assert(file->prev != NULL);
    file = file->prev;
    row = file->element_number;
    file_id.file = file;
    file_id.relative_row = row;
  }

#ifdef LOGS
  fprintf(stderr, "REMOVE LINE RELATIVE %d \r\n", row);
  fprintf(stderr, "CURRENT LINE ELEMENT NUMBER %d\r\n", file->element_number);
#endif
  assert(row >= 0);
  assert(row < file->element_number);

  if (row != file->element_number - 1) {
#ifdef LOGS
    fprintf(stderr, "REMOVE IN MIDDLE. Table Size %d. Element Number %d\r\n", file->current_max_element_number,
            file->element_number);
#endif
    // fprintf(stderr, "At move : %d, first %d, ")
    file->byte_count -= file->lines_byte_count[row] + 1;
    memmove_IntArray(file->lines_byte_count + row, file->lines_byte_count + row + 1, file->element_number - row - 1);
    memmove_LineNodeArray(file->lines + row, file->lines + row + 1, file->element_number - row - 1);
    rebindFileNode(file, row, file->element_number - 1 - row);
  }
  else {
#ifdef LOGS
    fprintf(stderr, "REMOVE ATT END\r\n");
#endif
    file->byte_count -= file->lines_byte_count[row] + 1;
  }

  file->element_number--;

  if (file->element_number == 0) {
    // the current node is empty.
    int available_prev = file->prev == NULL ? 0 : MAX_ELEMENT_NODE - file->prev->element_number;
    int available_next = file->next == NULL ? 0 : MAX_ELEMENT_NODE - file->next->element_number;

    if (file->prev != NULL && (available_prev != 0 || available_next != 0)) {
      assert(file->prev != NULL || file->next != NULL);
      // assert that the FileNode is not the root one.
      assert(file->prev != NULL);
#ifdef LOGS
      fprintf(stderr, "FREE NODE\r\n");
#endif
      file = destroyCurrentFileNode(file);
      file_id.file = file;
      file_id.relative_row = file_id.file->element_number;
    }
    else {
      if (file->current_max_element_number > CACHE_SIZE + 1) {
#ifdef LOGS
        fprintf(stderr, "REALLOC ONLY CACHE\r\n");
#endif
        file->current_max_element_number = min(MAX_ELEMENT_NODE, CACHE_SIZE);
        assert(file->current_max_element_number <= MAX_ELEMENT_NODE);
        LineNode* old_tab = file->lines;
        file->lines = realloc_LineNodeArray(file->lines, file->current_max_element_number);
        if (file->lines != old_tab) {
          rebindFullFileNode(file);
        }
      }
    }
  }
#ifdef LOGS
  fprintf(stderr, "REMOVE ENDED\r\n");
#endif
  return moduloFileIdentifier(file_id);
}

int getAbsoluteFileIndex(FileIdentifier id) {
  int abs = id.relative_row;
  id.file = id.file->prev;

  while (id.file != NULL) {
    abs += id.file->element_number;
    id.file = id.file->prev;
  }

  return abs;
}

LineNode* getLineForFileIdentifier(FileIdentifier file_id) {
  file_id = moduloFileIdentifier(file_id);
  if (file_id.relative_row == 0) {
    // assert(file_id.file->prev != NULL); // OUT OF RANGE.
    return file_id.file->prev->lines + file_id.file->prev->element_number - 1;
  }
  return file_id.file->lines + file_id.relative_row - 1;
}


bool checkFileIntegrity(FileNode* file) {
  while (file != NULL) {
    for (int i = 0; i < file->element_number; i++) {
      LineNode* line = file->lines + i;
      if (line != NULL) {
        // assert(line->prev == NULL);
        if (line->prev != NULL) {
          return false;
        }
      }
      while (line != NULL) {
        if (line->next != NULL) {
          // assert(line->next->prev == line);
          if (line->next->prev != line) {
            fprintf(stderr, "INTEGRITY WRONG \r\n");
            if (i != 0) {
              printLineNode(file->lines + i - 1);
              fprintf(stderr, "\r\n");
            }
            fprintf(stderr, "=> %d : ", line->element_number);
            printLineNode(line);
            fprintf(stderr, " -> ");
            if (line->next != NULL) {
              for (int j = 0; j < line->element_number; j++) {
                printChar_U8(stderr, line->ch[j]);
              }
            }
            else {
              fprintf(stderr, "None");
            }
            fprintf(stderr, "\r\n");
            // assert(line->next->prev == line);
            return false;
          }
        }
        line = line->next;
      }
    }
    if (file->next != NULL) {
      // assert(file->next->prev == file);
      if (file->next->prev != file) {
        return false;
      }
    }
    file = file->next;
  }
  return true;
}


bool printByteCount(FileNode* file) {
  while (file != NULL) {
    int file_total = 0;
    fprintf(stderr, "↓\n\n");
    for (int i = 0; i < file->element_number; i++) {
      fprintf(stderr, "%d =>  ", file->lines_byte_count[i]);
      LineNode* line = file->lines + i;
      int line_total = 0;
      while (line != NULL) {
        int count = byteCountForLineNode(line, 0, line->element_number);
        // for (int j = 0; j < line->element_number; j++) {
        // printChar_U8(stderr, line->ch[j]);
        // }
        // fprintf(stderr, " → ");
        fprintf(stderr, " %d → ", count);
        line_total += count;
        if (count != line->byte_count) {
          for (int j = 0; j < line->element_number; j++) {
            printChar_U8(stderr, line->ch[j]);
          }
          fprintf(stderr, " Got : %d, Saved : %d\n ", count, line->byte_count);
          assert(false); // SHOULD NOT HAPPEN !!!
        }
        line = line->next;
      }
      file_total += line_total;
      if (line_total != file->lines_byte_count[i]) {
        fprintf(stderr, "\nWrong\n");
      }
      else {
        fprintf(stderr, "\nRight\n");
      }
      file_total += 1;
    }

    for (int i = file->element_number; i < MAX_ELEMENT_NODE; i++) {
      fprintf(stderr, "%d =>  X\n", file->lines_byte_count[i]);
    }

    fprintf(stderr, "\nReel: %d\nSaved: %d\n", file_total, file->byte_count);
    if (file_total != file->byte_count) {
      fprintf(stderr, "\n\nWrong File\n");
    }
    else {
      fprintf(stderr, "\n\nRight File\n");
    }

    int table_sum = 0;
    for (int i = 0; i < file->element_number; i++) {
      table_sum += file->lines_byte_count[i];
    }

    // if (table_sum + file->element_number != file->byte_count) {
    // fprintf(stderr, "In TableSum got : %d Saved : %d\n", table_sum + file->element_number, file->byte_count);
    // return false;
    // }

    file = file->next;
  }

  return true;
}

bool checkByteCountIntegrity(FileNode* file) {
  while (file != NULL) {
    int file_total = 0;
    for (int i = 0; i < file->element_number; i++) {
      LineNode* line = file->lines + i;
      int line_total = 0;
      while (line != NULL) {
        int count = byteCountForLineNode(line, 0, line->element_number);
        line_total += count;
        if (count != line->byte_count) {
          fprintf(stderr, "In LineNode got : %d Saved : %d\n", count, line->byte_count);
          return false;
        }
        line = line->next;
      }
      file_total += line_total;
      if (line_total != file->lines_byte_count[i]) {
        fprintf(stderr, "In LineByteCount got : %d Saved : %d\n\r", line_total, file->lines_byte_count[i]);
        return false;
      }
    }
    file_total += file->element_number;
    if (file_total != file->byte_count) {
      fprintf(stderr, "In FileTotal got : %d Saved : %d\n\r", file_total, file->byte_count);
      return false;
    }

    int table_sum = 0;
    for (int i = 0; i < file->element_number; i++) {
      table_sum += file->lines_byte_count[i];
    }

    if (table_sum + file->element_number != file->byte_count) {
      fprintf(stderr, "In TableSum got : %d Saved : %d\n\r", table_sum + file->element_number, file->byte_count);
      return false;
    }

    file = file->next;
  }

  return true;
}

bool isEmptyFile(FileNode* file) {
  while (file != NULL) {
    if (file->element_number != 0) {
      return false;
    }
    file = file->next;
  }
  return true;
}

bool hasElementAfterFile(FileIdentifier file_id) {
  return file_id.relative_row != file_id.file->element_number || isEmptyFile(file_id.file->next) == false;
}

bool hasElementBeforeFile(FileIdentifier file_id) { return file_id.absolute_row != 1; }

FileIdentifier tryToReachAbsRow(FileIdentifier file_id, int abs_row) {
  if (file_id.absolute_row == abs_row) {
    return file_id;
  }

  file_id = moduloFileIdentifier(file_id);

  file_id.absolute_row -= file_id.relative_row;
  file_id.relative_row = 0;
  assert(file_id.relative_row == 0);


  bool first = false;

  while (file_id.absolute_row + file_id.file->element_number < abs_row) {
    first = true;
    if (file_id.file->next == NULL) {
      break;
    }
    file_id.absolute_row += file_id.file->element_number;
    file_id.file = file_id.file->next;
  }

  while (abs_row < file_id.absolute_row) {
    assert(first == false);
    if (file_id.file->prev == NULL) {
      break;
    }
    file_id.absolute_row -= file_id.file->prev->element_number;
    file_id.file = file_id.file->prev;
  }

  if (file_id.absolute_row < abs_row) {
    int toAdd = min(file_id.file->element_number - file_id.relative_row, abs_row - file_id.absolute_row);
    file_id.absolute_row += toAdd;
    file_id.relative_row += toAdd;
    assert(file_id.relative_row == file_id.file->element_number || abs_row == file_id.absolute_row);
  }

  return moduloFileIdentifier(file_id);
}


void deleteFilePart(FileIdentifier file_id, int length) {
#ifdef LOGS
  fprintf(stderr, "deleteFilePart\n");
  fprintf(stderr, " ====== DELETE FILE PART BEFORE =====\n");
  printByteCount(tryToReachAbsRow(file_id, 1).file);
  fprintf(stderr, " ====== DELETE FILE PART BEFORE END =====\n");
#endif
  file_id = moduloFileIdentifier(file_id);

  int current_removed = 0;
  while (current_removed < length) {
    // If it's the line end go to next.
    if (file_id.relative_row == file_id.file->element_number) {
      file_id.file = file_id.file->next;
      file_id.relative_row = 0;
      assert(file_id.file != NULL);
    }

    // If the id is the begin of a node and If the node can be completely removed. We can improve perf.
    if (file_id.relative_row == 0 && file_id.file->element_number <= length - current_removed) {
      current_removed += file_id.file->element_number;
      if (file_id.file->prev != NULL) {
        // If the node can be cleared.
        file_id.file = destroyCurrentFileNode(file_id.file);
        file_id.relative_row = file_id.file->element_number;
      }
      else {
        // If the node is fixed.
        // assert(false);
        file_id.file->element_number = 0;
        file_id.file->byte_count = 0;
        file_id.file->current_max_element_number = 0;
        for (int i = 0; i < file_id.file->element_number; i++) {
          destroyFullLine(file_id.file->lines + i);
        }
        free(file_id.file->lines);
        file_id.file->lines = NULL;
      }
    }
    else {
      // Need to delete a part of the node.
      int atDelete = min(length - current_removed, file_id.file->element_number - file_id.relative_row);
      for (int i = file_id.relative_row; i < file_id.relative_row + atDelete; i++) {
        destroyFullLine(file_id.file->lines + i);
      }
      int sum = sumLinesBytes(file_id.file->lines_byte_count + file_id.relative_row, atDelete);
      memmove_IntArray(file_id.file->lines_byte_count + file_id.relative_row,
                       file_id.file->lines_byte_count + file_id.relative_row + atDelete,
                       file_id.file->element_number - file_id.relative_row - atDelete);
      memmove_LineNodeArray(file_id.file->lines + file_id.relative_row,
                            file_id.file->lines + file_id.relative_row + atDelete,
                            file_id.file->element_number - file_id.relative_row - atDelete);
      file_id.file->byte_count -= sum + atDelete;

      file_id.file->element_number -= atDelete;
      current_removed += atDelete;


      // Realloc file.lines if too much unused mem.
      if (file_id.file->current_max_element_number > file_id.file->element_number + CACHE_SIZE) {
        file_id.file->lines = realloc_LineNodeArray(file_id.file->lines, file_id.file->element_number + CACHE_SIZE);
        file_id.file->current_max_element_number = file_id.file->element_number + CACHE_SIZE;
        assert(file_id.file->current_max_element_number <= MAX_ELEMENT_NODE);
        assert(file_id.file->element_number <= file_id.file->current_max_element_number);
      }

      rebindFileNode(file_id.file, 0, file_id.file->element_number);
    }
  }

  // fprintf(stderr, " ====== DELETE FILE PART AFTER =====\n");
  // printByteCount(tryToReachAbsRow(file_id, 1).file);
  // (stderr, " ====== DELETE FILE PART AFTER END =====\n");
}

/**
 * Destroy line free all memory.
 * Will check Node before.
 */
void destroyFullFile(FileNode* node) {
  while (node->prev != NULL) {
    node = node->prev;
  }

  while (node != NULL) {
    FileNode* tmp = node;
    node = node->next;

    for (int i = 0; i < tmp->element_number; i++) {
      destroyFullLine(tmp->lines + i);
    }

    free(tmp->lines);
    free(tmp);
  }
}


////// -------------- COMBO LINE & FILE --------------


Cursor moduloCursorR(FileNode* file, int row, int column) {
  FileIdentifier file_id = moduloFileIdentifierR(file, row);
  LineIdentifier line_id = moduloLineIdentifierR(getLineForFileIdentifier(file_id), column);

  return cursorOf(file_id, line_id);
}


/**
 *  Init a new head of FileNode.
 */
void initFileNode(FileNode* file, Size size);


/**
 *  Return the number of line in the FileNode.
 */
int sizeFileNode(FileNode* file) {
  int count = 0;
  while (file != NULL) {
    count += file->element_number;
    file = file->next;
  }
  return count;
}


/**
 * Return the char from the FileNode at index line, column. If file[line][column] don't exist return null.
 */
Char_U8 getCharAt(FileNode* file, int row, int column) {
  return getCharForLineIdentifier(moduloCursorR(file, row, column).line_id);
}

Cursor cursorOf(FileIdentifier file_id, LineIdentifier line_id) {
  Cursor cursor = {file_id, line_id};
  return moduloCursor(cursor);
}

Cursor moduloCursor(Cursor cursor) {
  cursor.file_id = moduloFileIdentifier(cursor.file_id);
  if (cursor.line_id.absolute_column <= MAX_ELEMENT_NODE) {
    // The node have a big chance to be fixed
    // So the risk that the line_id could have been reallocated is high.
    cursor.line_id = moduloLineIdentifierR(getLineForFileIdentifier(cursor.file_id), cursor.line_id.absolute_column);
  }
  else {
    // If the node is not fixed use classic modulo.
    // Less risk better performance.
    cursor.line_id = moduloLineIdentifier(cursor.line_id);
  }

  return cursor;
}

Cursor insertCharInLineC(Cursor cursor, Char_U8 ch) {
  cursor.line_id = insertCharInLine(cursor.line_id, ch);
  cursor = moduloCursor(cursor);
  assert(cursor.file_id.relative_row > 0 && cursor.file_id.relative_row <= MAX_ELEMENT_NODE);
  cursor.file_id.file->lines_byte_count[cursor.file_id.relative_row - 1] += utf8_size(ch);
  cursor.file_id.file->byte_count += utf8_size(ch);
  return cursor;
}

Cursor removeCharInLineC(Cursor cursor) {
  Char_U8 ch = getCharAtCursor(cursor);
  cursor.line_id = removeCharInLine(cursor.line_id);
  cursor = moduloCursor(cursor);
  assert(cursor.file_id.relative_row > 0 && cursor.file_id.relative_row <= MAX_ELEMENT_NODE);
  cursor.file_id.file->lines_byte_count[cursor.file_id.relative_row - 1] += utf8_size(ch);
  cursor.file_id.file->byte_count -= utf8_size(ch);
  return cursor;
}

Cursor removeLineInFileC(Cursor cursor) {
  destroyFullLine(getLineForFileIdentifier(cursor.file_id));
  FileIdentifier file_id_after_del = removeLineInFile(cursor.file_id);
  cursor.file_id = tryToReachAbsRow(file_id_after_del, file_id_after_del.absolute_row + 1);
  cursor.line_id = getLastLineNode(getLineForFileIdentifier(cursor.file_id));
  return cursor;
}


Cursor insertNewLineInLineC(Cursor cursor) {
  cursor = moduloCursor(cursor);

  FileIdentifier file_id = cursor.file_id;
  LineIdentifier line_id = cursor.line_id;
  const bool line_was_fixed = line_id.line->fixed;

  FileIdentifier newFileIdForNewLine = insertEmptyLineInFile(file_id);
  LineNode* newLine = getLineForFileIdentifier(newFileIdForNewLine);
  assert(newLine != NULL);
  assert(newLine->prev == NULL);
  assert(newLine->next == NULL);
  assert(newLine->fixed == true);
  assert(newLine->element_number == 0);
  assert(newLine->current_max_element_number == 0);
  assert(newLine->ch == NULL);
  assert(newLine->byte_count == 0);

  // file_id may have been reallocated.
  file_id = tryToReachAbsRow(newFileIdForNewLine, file_id.absolute_row);
  if (line_was_fixed) {
    // If the line was fixed the line may have been reallocated. So we need to re-use modulo. We admit that fixed are
    // the src of the line.
    line_id = moduloCursorR(file_id.file, file_id.relative_row, line_id.relative_column).line_id;
  }

  if (line_id.relative_column == 0) {
#ifdef LOGS
    fprintf(stderr, "Current cursor pos is 0 check to move current node.\r\n");
#endif

    // We are not moving the node for now but juste moving the Char_U8 array.
    newLine->ch = line_id.line->ch;
    newLine->next = line_id.line->next;
    newLine->current_max_element_number = line_id.line->current_max_element_number;
    newLine->element_number = line_id.line->element_number;
    newLine->byte_count = line_id.line->byte_count;

    // Updating byte_count and lines_byte_count for FileNode
    int count = file_id.file->lines_byte_count[file_id.relative_row - 1];
    assert(newFileIdForNewLine.file->lines_byte_count[newFileIdForNewLine.relative_row - 1] == 0);
    newFileIdForNewLine.file->lines_byte_count[newFileIdForNewLine.relative_row - 1] = count;
    file_id.file->lines_byte_count[file_id.relative_row - 1] = 0;
    newFileIdForNewLine.file->byte_count += count;
    file_id.file->byte_count -= count;


    if (line_id.line->fixed == false) {
      assert(false);
      // This situation is not supposed to happen, by the definition of modulo ! But it's implemented if needed
      if (line_id.line->prev != NULL) {
        line_id.line->prev->next = NULL;
        free(line_id.line);
      }
    }
    else {
      initEmptyLineNode(line_id.line);
      line_id.line->fixed = true;
    }
  }
  else if (line_id.relative_column == line_id.line->element_number) {
#ifdef LOGS
    fprintf(stderr, "Current cursor pos is max_element check to move next node.\r\n");
#endif
    // We are not moving the node for now but juste moving the Char_U8 array.
    if (line_id.line->next != NULL) {
      newLine->ch = line_id.line->next->ch;
      newLine->next = line_id.line->next->next;
      newLine->current_max_element_number = line_id.line->next->current_max_element_number;
      newLine->element_number = line_id.line->next->element_number;
      newLine->byte_count = line_id.line->next->byte_count;


      // fprintf(stderr, "NewLine : ");
      // for (int i  = 0 ; i< newLine->element_number; i++) {
      // printChar_U8(stderr, newLine->ch[i]);
      // }
      // fprintf(stderr, "\n");

      // Updating byte_count and lines_byte_count for FileNode
      int byte_moved_to_next_line = byteCountForCurrentLineToEnd(newLine, 0);
      // fprintf(stderr, "byte_moved : %d\n", byte_moved_to_next_line);
      assert(newFileIdForNewLine.file->lines_byte_count[newFileIdForNewLine.relative_row - 1] == 0);
      newFileIdForNewLine.file->lines_byte_count[newFileIdForNewLine.relative_row - 1] = byte_moved_to_next_line;
      file_id.file->lines_byte_count[file_id.relative_row - 1] -= byte_moved_to_next_line;
      newFileIdForNewLine.file->byte_count += byte_moved_to_next_line;
      file_id.file->byte_count -= byte_moved_to_next_line;

      if (line_id.line->next->fixed == false) {
#ifdef LOGS
        fprintf(stderr, "We can free.\n");
#endif
        free(line_id.line->next);
        line_id.line->next = NULL;
      }
      else {
#ifdef LOGS
        fprintf(stderr, "We cannot free.\n");
#endif
        initEmptyLineNode(line_id.line);
        line_id.line->fixed = true;
      }
    }
    else {
      // Currently at the end of the line.
      // DO NOTHING !
#ifdef LOGS
      fprintf(stderr, "Nothing to do ?\r\n");
#endif
    }
  }
  else {
    // In the middle of a line node. Need to copy data to a new linenode.
#ifdef LOGS
    fprintf(stderr, "Current cursor pos is in mid line dissociating line.\r\n");
#endif

    newLine->next = line_id.line->next;
    newLine->element_number = line_id.line->element_number - line_id.relative_column;
    newLine->byte_count = byteCountForLineNode(line_id.line, line_id.relative_column, newLine->element_number);

    newLine->current_max_element_number = min(newLine->element_number + CACHE_SIZE, MAX_ELEMENT_NODE);
    assert(newLine->current_max_element_number <= MAX_ELEMENT_NODE);

    assert(newLine->ch == NULL);
    newLine->ch = malloc_CharU8Array(newLine->current_max_element_number);
    memcpy_CharU8Array(newLine->ch, line_id.line->ch + line_id.relative_column, newLine->element_number);

    line_id.line->next = NULL;
    line_id.line->element_number = line_id.relative_column;
    line_id.line->byte_count -= newLine->byte_count;


    // Updating byte_count and lines_byte_count for FileNode
    int byte_moved_to_next_line = byteCountForCurrentLineToEnd(newLine, 0);
    assert(newFileIdForNewLine.file->lines_byte_count[newFileIdForNewLine.relative_row - 1] == 0);
    newFileIdForNewLine.file->lines_byte_count[newFileIdForNewLine.relative_row - 1] = byte_moved_to_next_line;
    file_id.file->lines_byte_count[file_id.relative_row - 1] -= byte_moved_to_next_line;
    newFileIdForNewLine.file->byte_count += byte_moved_to_next_line;
    file_id.file->byte_count -= byte_moved_to_next_line;

    const int old_max_element_number = line_id.line->current_max_element_number;
    line_id.line->current_max_element_number =
      min(min(MAX_ELEMENT_NODE, old_max_element_number), line_id.line->element_number + CACHE_SIZE);
    if (old_max_element_number != line_id.line->current_max_element_number) {
      line_id.line->ch = realloc_CharU8Array(line_id.line->ch, line_id.line->current_max_element_number);
    }
  }

  if (newLine->next != NULL) { // Really important because newLine may have changed.
    newLine->next->prev = newLine;
  }

  LineIdentifier newLineId = moduloLineIdentifierR(newLine, 0);
  return cursorOf(newFileIdForNewLine, newLineId);
}


/**
 *
 * Dumb implementation of concat.
 * */
Cursor concatNeighbordsLinesC(Cursor cursor) {
  cursor = moduloCursor(cursor);
  // fprintf(stderr, "Concat\n");

  FileIdentifier file_id = cursor.file_id;
  LineIdentifier line_id = cursor.line_id;

  assert(line_id.line->fixed == true);
  // We assume that in the file we remove a line when we are at the column 0 so the lineNode is fixed.

  if (isEmptyLine(line_id.line)) {
#ifdef LOGS
    fprintf(stderr, "Deleting without moving empty line\r\n");
#endif

    destroyFullLine(getLineForFileIdentifier(file_id));

    FileIdentifier newLineId = removeLineInFile(file_id);
    LineIdentifier lastNode = getLastLineNode(getLineForFileIdentifier(newLineId));

    if (getLineForFileIdentifier(newLineId)->next != NULL) {
      assert(lastNode.line != NULL);
    }

    return cursorOf(newLineId, lastNode);
  }
#ifdef LOGS
  fprintf(stderr, "Deleting with moving line\r\n");
#endif

  LineNode* newNode = malloc_LineNodeArray(1);
  initEmptyLineNode(newNode);
  newNode->ch = line_id.line->ch;
  newNode->next = line_id.line->next;
  if (newNode->next != NULL) {
    newNode->next->prev = newNode;
  }
  newNode->fixed = false;
  newNode->element_number = line_id.line->element_number;
  newNode->current_max_element_number = line_id.line->current_max_element_number;
  newNode->byte_count = line_id.line->byte_count;

  int byte_moved = file_id.file->lines_byte_count[file_id.relative_row - 1];
  assert(byte_moved == byteCountForCurrentLineToEnd(newNode, 0));
  int old_byte_count = file_id.file->byte_count;
  FileIdentifier newLineId = removeLineInFile(file_id);
  // fprintf(stderr, "Old %d\nNew: %d\nLine Size: %d\n", old_byte_count, file_id.file->byte_count, byte_moved);
  // assert(file_id.file->byte_count == old_byte_count - byte_moved - 1); // may cause use after free at remove.
  LineIdentifier lastNode = getLastLineNode(getLineForFileIdentifier(newLineId));

  if (lastNode.line->element_number == 0) {
    // Just copy the data of newNode to this node.
#ifdef LOGS
    fprintf(stderr, "LAST EMTPY COPYING\r\n");
#endif
    assert(lastNode.line->element_number == 0);
    free(lastNode.line->ch);
    lastNode.line->ch = newNode->ch;
    lastNode.line->next = newNode->next;
    lastNode.line->element_number = newNode->element_number;
    lastNode.line->current_max_element_number = newNode->current_max_element_number;
    lastNode.line->byte_count = newNode->byte_count;
    if (lastNode.line->next != NULL) {
      lastNode.line->next->prev = lastNode.line;
    }
    free(newNode);
  }
  else {
#ifdef LOGS
    fprintf(stderr, "HAPPENING\r\n");
#endif
    lastNode.line->next = newNode;
    newNode->prev = lastNode.line;
  }

  newLineId.file->lines_byte_count[newLineId.relative_row - 1] += byte_moved;
  newLineId.file->byte_count += byte_moved;

#ifdef LOGS
  // cursor = cursorOf(newLineId, lastNode);
  // fprintf(stderr, "BEFORE ERROR :\n");
  // printByteCount(cursor.file_id.file);
  // cursor = tryToReachAbsPosition(cursor, 1, 0);
  // assert(checkFileIntegrity(cursor.file_id.file));
  // assert(checkByteCountIntegrity(cursor.file_id.file));
#endif

  return cursorOf(newLineId, lastNode);
}

Cursor moveRight_internal(Cursor cursor) {
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

Cursor deleteCharAtCursor_v2(Cursor cursor) {
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

Cursor supprCharAtCursor_internal(Cursor cursor) {
  Cursor old_cur = cursor;
  cursor = moveRight_internal(cursor);
  if (old_cur.file_id.absolute_row != cursor.file_id.absolute_row ||
      old_cur.line_id.absolute_column != cursor.line_id.absolute_column) {
    cursor = deleteCharAtCursor_v2(cursor);
  }
  return cursor;
}

Cursor bulkDelete(Cursor cursor, Cursor select_cursor) {
  if (cursor_le(select_cursor, cursor)) {
    Cursor tmp = select_cursor;
    select_cursor = cursor;
    cursor = tmp;
  }

  cursor = moduloCursor(cursor);
  select_cursor = moduloCursor(select_cursor);

  if (cursor.file_id.absolute_row == select_cursor.file_id.absolute_row) {
    // Need to delete part of a line.
    int old_byte_count = cursor.file_id.file->lines_byte_count[cursor.file_id.relative_row - 1];
    deleteLinePart(cursor.line_id, select_cursor.line_id.absolute_column - cursor.line_id.absolute_column);
    int new_byte_count = byteCountForCurrentLineToEnd(getLineForFileIdentifier(cursor.file_id), 0);
    cursor.file_id.file->lines_byte_count[cursor.file_id.relative_row - 1] = new_byte_count;
    cursor.file_id.file->byte_count -= old_byte_count - new_byte_count;
  }
  else {
    Cursor test = tryToReachAbsPosition(cursor, 1, 0);
    int old_byte_count = cursor.file_id.file->lines_byte_count[cursor.file_id.relative_row - 1];
    deleteLinePart(cursor.line_id,
                   tryToReachAbsColumn(cursor.line_id, INT_MAX).absolute_column - cursor.line_id.absolute_column);
    int new_byte_count = byteCountForCurrentLineToEnd(getLineForFileIdentifier(cursor.file_id), 0);
    cursor.file_id.file->lines_byte_count[cursor.file_id.relative_row - 1] = new_byte_count;
    cursor.file_id.file->byte_count -= old_byte_count - new_byte_count;


    old_byte_count = select_cursor.file_id.file->lines_byte_count[select_cursor.file_id.relative_row - 1];
    deleteLinePart(tryToReachAbsColumn(select_cursor.line_id, 0), select_cursor.line_id.absolute_column);
    new_byte_count = byteCountForCurrentLineToEnd(getLineForFileIdentifier(select_cursor.file_id), 0);
    select_cursor.file_id.file->lines_byte_count[select_cursor.file_id.relative_row - 1] = new_byte_count;
    select_cursor.file_id.file->byte_count -= old_byte_count - new_byte_count;


    deleteFilePart(tryToReachAbsRow(cursor.file_id, cursor.file_id.absolute_row),
                   select_cursor.file_id.absolute_row - cursor.file_id.absolute_row - 1);
    cursor = moduloCursor(cursor);
    cursor = supprCharAtCursor_internal(cursor);
  }

  return cursor;
}


Cursor tryToReachAbsPosition(Cursor cursor, int row, int column) {
  if (row <= 0) {
    row = 1;
  }
  FileIdentifier new_file_id = tryToReachAbsRow(cursor.file_id, row);
  // if the preferred row isn't found adapt the preferred column
  if (new_file_id.absolute_row < row) {
    column = INT_MAX;
  }
  if (new_file_id.absolute_row > row) {
    column = 0;
  }
  LineIdentifier new_line_id =
    tryToReachAbsColumn(moduloLineIdentifierR(getLineForFileIdentifier(new_file_id), 0), column);
  return cursorOf(new_file_id, new_line_id);
}

Char_U8 getCharAtCursor(Cursor cursor) {
  if (cursor_col(cursor) == 0) {
    return readChar_U8FromCharArray("\n");
  }
  return getCharForLineIdentifier(cursor.line_id);
}

// --- Cursor Operations ---

int cursor_cmp(Cursor c1, Cursor c2) {
  if (c1.file_id.absolute_row < c2.file_id.absolute_row) {
    return -1;
  }
  if (c1.file_id.absolute_row > c2.file_id.absolute_row) {
    return 1;
  }
  if (c1.line_id.absolute_column < c2.line_id.absolute_column) {
    return -1;
  }
  if (c1.line_id.absolute_column > c2.line_id.absolute_column) {
    return 1;
  }
  return 0;
}

bool cursor_eq(Cursor c1, Cursor c2) { return cursor_cmp(c1, c2) == 0; }
bool cursor_ne(Cursor c1, Cursor c2) { return cursor_cmp(c1, c2) != 0; }
bool cursor_lt(Cursor c1, Cursor c2) { return cursor_cmp(c1, c2) < 0; }
bool cursor_le(Cursor c1, Cursor c2) { return cursor_cmp(c1, c2) <= 0; }
bool cursor_gt(Cursor c1, Cursor c2) { return cursor_cmp(c1, c2) > 0; }
bool cursor_ge(Cursor c1, Cursor c2) { return cursor_cmp(c1, c2) >= 0; }

bool cursor_is_between(Cursor c, Cursor cur1, Cursor cur2) {
  Cursor start = cursor_min(cur1, cur2);
  Cursor end = cursor_max(cur1, cur2);
  return cursor_ge(c, start) && cursor_le(c, end);
}

bool cursor_is_inside(Cursor c, Cursor cur1, Cursor cur2) {
  Cursor start = cursor_min(cur1, cur2);
  Cursor end = cursor_max(cur1, cur2);
  return cursor_lt(start, c) && cursor_le(c, end);
}

// --- CursorDescriptor Operations ---

int cursor_desc_cmp(CursorDescriptor c1, CursorDescriptor c2) {
  if (c1.row < c2.row) {
    return -1;
  }
  if (c1.row > c2.row) {
    return 1;
  }
  if (c1.column < c2.column) {
    return -1;
  }
  if (c1.column > c2.column) {
    return 1;
  }
  return 0;
}

bool cursor_desc_eq(CursorDescriptor c1, CursorDescriptor c2) { return cursor_desc_cmp(c1, c2) == 0; }
bool cursor_desc_ne(CursorDescriptor c1, CursorDescriptor c2) { return cursor_desc_cmp(c1, c2) != 0; }
bool cursor_desc_lt(CursorDescriptor c1, CursorDescriptor c2) { return cursor_desc_cmp(c1, c2) < 0; }
bool cursor_desc_le(CursorDescriptor c1, CursorDescriptor c2) { return cursor_desc_cmp(c1, c2) <= 0; }
bool cursor_desc_gt(CursorDescriptor c1, CursorDescriptor c2) { return cursor_desc_cmp(c1, c2) > 0; }
bool cursor_desc_ge(CursorDescriptor c1, CursorDescriptor c2) { return cursor_desc_cmp(c1, c2) >= 0; }

bool cursor_desc_is_between(CursorDescriptor cd, CursorDescriptor cur1, CursorDescriptor cur2) {
  CursorDescriptor start = cursor_desc_min(cur1, cur2);
  CursorDescriptor end = cursor_desc_max(cur1, cur2);
  return cursor_desc_ge(cd, start) && cursor_desc_le(cd, end);
}

// --- Status ---

bool cursor_is_disabled(Cursor c) { return c.file_id.absolute_row == -1; }
bool cursor_desc_is_disabled(CursorDescriptor cd) { return cd.row == -1; }
Cursor cursor_disable(Cursor c) {
  c.file_id.absolute_row = -1;
  return c;
}

int cursor_row(Cursor c) { return c.file_id.absolute_row; }
int cursor_col(Cursor c) { return c.line_id.absolute_column; }

// --- Utilities ---

Cursor cursor_min(Cursor c1, Cursor c2) { return cursor_le(c1, c2) ? c1 : c2; }
Cursor cursor_max(Cursor c1, Cursor c2) { return cursor_ge(c1, c2) ? c1 : c2; }

CursorDescriptor cursor_desc_min(CursorDescriptor c1, CursorDescriptor c2) { return cursor_desc_le(c1, c2) ? c1 : c2; }
CursorDescriptor cursor_desc_max(CursorDescriptor c1, CursorDescriptor c2) { return cursor_desc_ge(c1, c2) ? c1 : c2; }

// --- Conversion ---

CursorDescriptor cursor_to_desc(Cursor cursor) {
  CursorDescriptor descriptor;
  descriptor.row = cursor.file_id.absolute_row;
  descriptor.column = cursor.line_id.absolute_column;
  return descriptor;
}

Cursor desc_to_cursor(Cursor base, CursorDescriptor descriptor) {
  return tryToReachAbsPosition(base, descriptor.row, descriptor.column);
}


unsigned int getIndexForCursor(Cursor cursor) {
  cursor = moduloCursor(cursor);
  FileNode* current_file = cursor.file_id.file;
  unsigned int index = 0;

  // Adding previous nodes byte_count
  current_file = current_file->prev;
  while (current_file != NULL) {
    index += current_file->byte_count;
    current_file = current_file->prev;
  }

  // Adding previous line byte_count
  for (int i = 0; i < cursor.file_id.relative_row - 1; i++) {
    index += cursor.file_id.file->lines_byte_count[i] + 1;
  }

  // Adding previous LineNode byte_count
  LineNode* current_line = getLineForFileIdentifier(cursor.file_id);
  while (current_line != cursor.line_id.line) {
    index += current_line->byte_count;
    current_line = current_line->next;
  }

  // Adding current linenode chars
  for (int i = 0; i < cursor.line_id.relative_column; i++) {
    index += utf8_size(cursor.line_id.line->ch[i]);
  }

  return index;
}

Cursor getCursorForIndex(Cursor cursor, unsigned int index) {
  Cursor cursor_at_index;
  int abs_row = 0;
  int abs_column = 0;

  FileNode* file_node = cursor.file_id.file;
  while (file_node->prev != NULL) {
    file_node = file_node->prev;
  }

  int current_index = 0;
  while (current_index + file_node->byte_count < index) {
    current_index += file_node->byte_count;
    file_node = file_node->next;
    abs_row += file_node->element_number;
    assert(file_node != NULL); // INDEX OUT OF BOUNDS.
  }

  int i = 0;
  while (current_index + file_node->lines_byte_count[i] + 1 < index) {
    current_index += file_node->lines_byte_count[i] + 1;
    i++;
    assert(i < file_node->element_number);
  }

  assert(i < file_node->element_number);
  abs_row += i;

  LineNode* line_node = file_node->lines + i;
  while (line_node->next != NULL && current_index + line_node->byte_count < index) {
    current_index += line_node->byte_count;
    abs_column += line_node->element_number;
    line_node = line_node->next;
  }

  int j = 0;
  int saved_size = 0;
  while (j < line_node->element_number && current_index + (saved_size = utf8_size(line_node->ch[j])) < index) {
    current_index += saved_size;
    j++;
  }

  abs_column += j;

  cursor_at_index.file_id.absolute_row = abs_row;
  cursor_at_index.file_id.file = file_node;
  cursor_at_index.file_id.relative_row = i;

  cursor_at_index.line_id.absolute_column = abs_column;
  cursor_at_index.line_id.line = line_node;
  cursor_at_index.line_id.relative_column = j;

  return cursor_at_index;
}

int readNu8CharAtCursor(Cursor* cursor_p, char* dest, int utf8_char_length) {
  Cursor cursor = moduloCursor(*cursor_p);
  int read = 0;
  int buff_length = 0;
  while (read < utf8_char_length) {
    if (cursor.line_id.line->element_number == cursor.line_id.relative_column) {
      if (hasElementAfterLine(cursor.line_id)) {
        // has next in the line.
        assert(cursor.line_id.line->next != NULL);
        cursor.line_id.line = cursor.line_id.line->next;
        cursor.line_id.relative_column = 0;
      }
      else if (hasElementAfterFile(cursor.file_id) == false) {
        // cursor currently at the end of the current file.
        break;
      }
      else {
        int old_cur_row = cursor.file_id.absolute_row;
        cursor = moveRight_internal(cursor);
        if (cursor.file_id.absolute_row + 1 == old_cur_row) {
          // EOF
          break;
        }
        dest[buff_length] = '\n';
        buff_length++;
        read++;
      }
      continue;
    }

    char temp_char;
    while (cursor.line_id.relative_column < cursor.line_id.line->element_number && read < utf8_char_length) {
      // We assume that their is mainly ascii char so to improve perf we are using this condition internaly.
      if (((temp_char = cursor.line_id.line->ch[cursor.line_id.relative_column].t[0]) >> 7 & 0b1) == 0) {
        dest[buff_length] = temp_char;
        buff_length++;
      }
      else {
        for (int i = 0; i < utf8_size(cursor.line_id.line->ch[cursor.line_id.relative_column]); i++) {
          dest[buff_length] = cursor.line_id.line->ch[cursor.line_id.relative_column].t[i];
          buff_length++;
        }
      }
      read++;
      cursor.line_id.relative_column++;
      cursor.line_id.absolute_column++;
    }
  }
  *cursor_p = cursor;
  return buff_length;
}

int readNu8CharAtPosition(Cursor* cursor_p, int row_raw, int column_raw, char* dest, int utf8_char_length) {
  int row = row_raw + 1;
  FileIdentifier file_id = tryToReachAbsRow(cursor_p->file_id, row);
  LineIdentifier line_id = moduloLineIdentifierR(getLineForFileIdentifier(file_id), 0);

  // reach column_raw.
  int current_column_raw = 0;
  while (current_column_raw < column_raw) {
    // if we can skip current node.
    if (current_column_raw + line_id.line->byte_count < column_raw) {
      line_id.relative_column = 0;
      line_id.absolute_column += line_id.line->element_number;
      current_column_raw += line_id.line->byte_count;
      line_id.line = line_id.line->next;
      assert(line_id.line != NULL); // INDEX OUT OF RANGE
    }
    else {
      while (current_column_raw < column_raw) {
        current_column_raw += utf8_size(line_id.line->ch[line_id.relative_column]);
        line_id.relative_column++;
        line_id.absolute_column++;
      }
    }
  }
  assert(current_column_raw == column_raw);

  *cursor_p = cursorOf(file_id, line_id);
  return readNu8CharAtCursor(cursor_p, dest, utf8_char_length);
}


int readNu8CharAtIndex(Cursor* cursor_p, int byte_index, char* dest, int utf8_char_length) {
  *cursor_p = getCursorForIndex(*cursor_p, byte_index);
  return readNu8CharAtCursor(cursor_p, dest, utf8_char_length);
}


int readNBytesCharAtCursor(Cursor* cursor_p, char* dest, int length) {
  Cursor cursor = moduloCursor(*cursor_p);
  int read = 0;
  int buff_length = 0;
  while (buff_length < length) {
    if (cursor.line_id.line->element_number == cursor.line_id.relative_column) {
      if (hasElementAfterLine(cursor.line_id)) {
        // has next in the line.
        assert(cursor.line_id.line->next != NULL);
        cursor.line_id.line = cursor.line_id.line->next;
        cursor.line_id.relative_column = 0;
      }
      else if (hasElementAfterFile(cursor.file_id) == false) {
        // cursor currently at the end of the current file.
        break;
      }
      else {
        int old_cur_row = cursor.file_id.absolute_row;
        cursor = moveRight_internal(cursor);
        if (cursor.file_id.absolute_row + 1 == old_cur_row) {
          // EOF
          break;
        }
        dest[buff_length] = '\n';
        buff_length++;
        read++;
      }
      continue;
    }

    char temp_char;
    while (cursor.line_id.relative_column < cursor.line_id.line->element_number && buff_length < length) {
      // We assume that their is mainly ascii char so to improve perf we are using this condition internaly.
      if (((temp_char = cursor.line_id.line->ch[cursor.line_id.relative_column].t[0]) >> 7 & 0b1) == 0) {
        dest[buff_length] = temp_char;
        buff_length++;
      }
      else {
        for (int i = 0; i < utf8_size(cursor.line_id.line->ch[cursor.line_id.relative_column]); i++) {
          dest[buff_length] = cursor.line_id.line->ch[cursor.line_id.relative_column].t[i];
          buff_length++;
        }
      }
      read++;
      cursor.line_id.relative_column++;
      cursor.line_id.absolute_column++;
    }
  }
  *cursor_p = cursor;
  return buff_length;
}

int readNBytesAtPosition(Cursor* cursor_p, int row_raw, int column_raw, char* dest, int length) {
  int row = row_raw + 1;
  FileIdentifier file_id = tryToReachAbsRow(cursor_p->file_id, row);
  LineIdentifier line_id = moduloLineIdentifierR(getLineForFileIdentifier(file_id), 0);

  // reach column_raw.
  // try to convert raw column to cursor column.
  int current_column_raw = 0;
  while (current_column_raw < column_raw) {
    // if we can skip current node.
    if (current_column_raw + line_id.line->byte_count < column_raw) {
      line_id.relative_column = 0;
      line_id.absolute_column += line_id.line->element_number;
      current_column_raw += line_id.line->byte_count;
      line_id.line = line_id.line->next;
      assert(line_id.line != NULL);
      // INDEX OUT OF RANGE
    }
    else {
      while (current_column_raw < column_raw) {
        current_column_raw += utf8_size(line_id.line->ch[line_id.relative_column]);
        line_id.relative_column++;
        line_id.absolute_column++;
      }
    }
  }
  assert(current_column_raw == column_raw);

  *cursor_p = cursorOf(file_id, line_id);
  return readNBytesCharAtCursor(cursor_p, dest, length);
}
