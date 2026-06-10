#include "io_manager.h"
#include "../environnement/constants.h"

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


bool loadFile(Cursor cursor, char* fileName, LF_Tabulation* tab) {
  cursor = moduloCursor(cursor);
  // If relative_row == 0 no line created. Use initNewWrittableFile() before.
  assert(cursor.file_id.relative_row != 0);

  FILE* f = fopen(fileName, "r");
  if (f == NULL) {
    printf("Coudln't open file %s !\r\n", fileName);
    return false;
  }


  // Duplicated search in project DUP_SCAN.
  char c;
  while (fscanf(f, "%c", &c) != EOF) {
#ifdef LOGS
    // assert(checkFileIntegrity(root) == true);
#endif
    if (iscntrl(c)) {
      if (c == '\n') {
#ifdef LOGS
        // fprintf(stderr, "Enter\r\n");
#endif
        cursor = insertNewLineInLineC(cursor);
      }
      else if (c == 9) {
#ifdef LOGS
        // fprintf(stderr, "Tab\r\n");
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
      Char_U8 ch = readChar_U8FromFileWithFirst(f, c);
#ifdef LOGS
      printChar_U8(stdout, ch);
      // printf("\r\n");
#endif
      cursor = insertCharInLineC(cursor, ch);
    }
  }

  fclose(f);

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
