#ifndef IO_EXPLORER_H
#define IO_EXPLORER_H
#include <limits.h>
#include <stdbool.h>


typedef struct {
  char path[PATH_MAX];
  int size;
} ExplorerFile;

struct _ExplorerFolder {
  char path[PATH_MAX];
  struct _ExplorerFolder* folders;
  ExplorerFile* files;
  int folder_count;
  int file_count;
  bool discovered;
  bool open;
};

typedef struct _ExplorerFolder ExplorerFolder;

void initFolder(char* path, ExplorerFolder* folder);

void discoverFolder(ExplorerFolder* folder);

void destroyFolder(ExplorerFolder* folder);

void reloadFolder(ExplorerFolder* folder);

#endif // IO_EXPLORER_H

