#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H
#include <linux/limits.h>

#include "../config/language_feature.h"
#include "../data-management/file_structure.h"

typedef enum {
  NONE,
  DONT_EXIST,
  EXIST,
} FILE_STATUS;


typedef struct {
  FILE_STATUS status;
  char path_abs[PATH_MAX];
  char path_args[PATH_MAX];
} IO_FileID;

Cursor initWrittableFileFromFile(char* fileName, LF_Tabulation* tab);

bool loadFile(Cursor cursor, char* fileName, LF_Tabulation* tab);

bool saveFile(FileNode* root, IO_FileID* file);

void setupFile(char* path, IO_FileID* file);

#endif // FILE_MANAGER_H
