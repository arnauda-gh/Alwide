#include "io_manager.h"
#include "../environnement/constants.h"
#include "../utils/tools.h"
#include "../data-management/file_structure.h"

#include <assert.h>
#include <ctype.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

Cursor initWrittableFileFromFile(char* fileName, LF_Tabulation* tab) {
  Cursor cursor = initNewWrittableFile();
  loadFile(cursor, fileName, tab);
  return cursorOf(cursor.file_id, moduloLineIdentifierR(getLineForFileIdentifier(cursor.file_id), 0));
}


static void append_newline(FileNode** active_file_p, int* active_line_idx_p, LineNode** active_line_node_p) {
  FileNode* active_file = *active_file_p;
  int active_line_idx = *active_line_idx_p;
  
  int total_bytes = 0;
  LineNode* seg = active_file->lines + active_line_idx;
  while (seg != NULL) {
    total_bytes += seg->byte_count;
    seg = seg->next;
  }
  active_file->lines_byte_count[active_line_idx] = total_bytes;

  active_line_idx++;
  
  if (active_line_idx < MAX_ELEMENT_NODE) {
    if (active_line_idx >= active_file->current_max_element_number) {
      active_file->current_max_element_number = min(active_file->current_max_element_number + CACHE_SIZE, MAX_ELEMENT_NODE);
      LineNode* old_tab = active_file->lines;
      active_file->lines = realloc_LineNodeArray(active_file->lines, active_file->current_max_element_number);
      if (active_file->lines != old_tab) {
        rebindFullFileNode(active_file);
      }
    }
    
    LineNode* new_line = active_file->lines + active_line_idx;
    initEmptyLineNode(new_line);
    new_line->fixed = true;
    active_file->element_number = active_line_idx + 1;
    
    *active_line_idx_p = active_line_idx;
    *active_line_node_p = new_line;
  }
  else {
    FileNode* next_file = malloc_FileNodeArray(1);
    initEmptyFileNode(next_file);
    
    active_file->next = next_file;
    next_file->prev = active_file;
    
    next_file->current_max_element_number = CACHE_SIZE;
    next_file->lines = malloc_LineNodeArray(CACHE_SIZE);
    
    LineNode* new_line = next_file->lines;
    initEmptyLineNode(new_line);
    new_line->fixed = true;
    next_file->element_number = 1;
    
    *active_file_p = next_file;
    *active_line_idx_p = 0;
    *active_line_node_p = new_line;
  }
}

static void append_char(LineNode** active_line_node_p, Char_U8 ch) {
  LineNode* active_line_node = *active_line_node_p;
  
  if (active_line_node->element_number == MAX_ELEMENT_NODE) {
    LineNode* next_node = malloc_LineNodeArray(1);
    initEmptyLineNode(next_node);
    
    active_line_node->next = next_node;
    next_node->prev = active_line_node;
    
    active_line_node = next_node;
    *active_line_node_p = active_line_node;
  }
  
  if (active_line_node->element_number >= active_line_node->current_max_element_number) {
    active_line_node->current_max_element_number = min(active_line_node->current_max_element_number + CACHE_SIZE, MAX_ELEMENT_NODE);
    active_line_node->ch = realloc_CharU8Array(active_line_node->ch, active_line_node->current_max_element_number);
  }
  
  active_line_node->ch[active_line_node->element_number] = ch;
  active_line_node->element_number++;
  active_line_node->byte_count += utf8_size(ch);
}

bool loadFile(Cursor cursor, char* fileName, LF_Tabulation* tab) {
  FILE* f = fopen(fileName, "rb");
  if (f == NULL) {
    printf("Couldn't open file %s !\r\n", fileName);
    return false;
  }

  fseek(f, 0, SEEK_END);
  long file_size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char* buf = malloc(file_size + 1);
  if (!buf) {
    fclose(f);
    return false;
  }

  long read_bytes = fread(buf, 1, file_size, f);
  buf[read_bytes] = '\0';
  fclose(f);

  FileNode* active_file = cursor.file_id.file;
  int active_line_idx = 0;
  LineNode* active_line = active_file->lines + active_line_idx;
  LineNode* active_line_node = active_line;

  long offset = 0;
  while (offset < read_bytes) {
    char c = buf[offset];
    if (iscntrl(c)) {
      if (c == '\n') {
        append_newline(&active_file, &active_line_idx, &active_line_node);
        offset++;
      }
      else if (c == 9) {
        Char_U8 ch;
        if (!tab->use_space) {
          ch.t[0] = '\t';
          for (int i = 1; i < 4; i++) ch.t[i] = 0;
          append_char(&active_line_node, ch);
        }
        else {
          ch.t[0] = ' ';
          for (int i = 1; i < 4; i++) ch.t[i] = 0;
          for (int i = 0; i < tab->size; i++) {
            append_char(&active_line_node, ch);
          }
        }
        offset++;
      }
      else {
        // Skip other control chars
        offset++;
      }
    }
    else {
      Char_U8 ch = readChar_U8FromCharArrayWithFirst(buf + offset, c);
      append_char(&active_line_node, ch);
      offset += utf8_size(ch);
    }
  }

  free(buf);

  // Complete last line
  int total_bytes = 0;
  LineNode* seg = active_file->lines + active_line_idx;
  while (seg != NULL) {
    total_bytes += seg->byte_count;
    seg = seg->next;
  }
  active_file->lines_byte_count[active_line_idx] = total_bytes;

  // Recalculate file nodes byte count
  FileNode* fn = cursor.file_id.file;
  while (fn != NULL) {
    int sum = 0;
    for (int i = 0; i < fn->element_number; i++) {
      sum += fn->lines_byte_count[i];
    }
    fn->byte_count = sum + fn->element_number;
    fn = fn->next;
  }

  // Debug check
  fn = cursor.file_id.file;
  while (fn != NULL) {
    for (int i = 0; i < fn->element_number; i++) {
      if (fn->lines[i].prev != NULL) {
        fprintf(stderr, "WARNING: fn->lines[%d].prev is NOT NULL! it is %p, fn->lines is %p\n", i, (void*)fn->lines[i].prev, (void*)fn->lines);
      }
    }
    fn = fn->next;
  }

  return true;
}

bool saveFile(FileNode* root, IO_FileID* file) {
  if (file->status == DONT_EXIST) {
    // create the file.
    FILE* tmp_file = fopen(file->path_args, "w");
    if (!tmp_file) {
      return false;
    }
    fclose(tmp_file);
    char* realpath_resulst = realpath(file->path_args, file->path_abs);
    assert(realpath_resulst != NULL);
    file->status = EXIST;
  }

  assert(file->status == EXIST);
  FILE* f = fopen(file->path_abs, "w");
  if (!f) {
    return false;
  }
  bool first = true;
  while (root != NULL) {
    for (int i = 0; i < root->element_number; i++) {
      if (!first) {
        fprintf(f, "\n");
      }
      else {
        first = false;
      }
      LineNode* line = root->lines + i;
      while (line != NULL) {
        for (int j = 0; j < line->element_number; j++) {
          printChar_U8(f, line->ch[j]);
        }
        line = line->next;
      }
    }
    root = root->next;
  }


  fclose(f);
  return true;
}


void setupFile(char* path, IO_FileID* file) {
  // 3 file status: no file given, file doesn't exist, file exists.
  if (strcmp(path, "") != 0) {
    strncpy(file->path_args, path, PATH_MAX);
    if (access(path, F_OK) == 0) {
      file->status = EXIST;
      // File exist
      char* realpath_result = realpath(path, file->path_abs);
      assert(realpath_result != NULL);
    }
    else {
      // File doesn't exist.
      file->status = DONT_EXIST;
      char path_copy1[PATH_MAX];
      char path_copy2[PATH_MAX];
      strncpy(path_copy1, path, PATH_MAX);
      strncpy(path_copy2, path, PATH_MAX);

      char* dname = dirname(path_copy1);
      char* bname = basename(path_copy2);

      char dir_abs[PATH_MAX];
      if (realpath(dname, dir_abs) != NULL) {
        snprintf(file->path_abs, PATH_MAX, "%s/%s", dir_abs, bname);
      }
      else {
        // Fallback
        if (path[0] == '/') {
          strncpy(file->path_abs, path, PATH_MAX);
        }
        else {
          char cwd[PATH_MAX];
          getcwd(cwd, sizeof(cwd));
          snprintf(file->path_abs, PATH_MAX, "%s/%s", cwd, path);
        }
      }
    }
  }
  else {
    // No file given
    file->status = NONE;
    strcpy(file->path_args, "untitled");
    strcpy(file->path_abs, "untitled");
  }
}
