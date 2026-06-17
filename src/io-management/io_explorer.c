#include "io_explorer.h"

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void initFolder(char* path, ExplorerFolder* folder) {
  folder->folders = NULL;
  folder->files = NULL;
  folder->folder_count = 0;
  folder->file_count = 0;
  strncpy(folder->path, path, PATH_MAX);
  folder->open = false;
  folder->discovered = false;
}

// strcmp with out case sensitive
int strcicmp(char const* a, char const* b) {
  for (;; a++, b++) {
    int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
    if (d != 0 || !*a) {
      return d;
    }
  }
}

void discoverFolder(ExplorerFolder* folder) {
  assert(folder != NULL);

  DIR* d;
  struct dirent* dir;
  d = opendir(folder->path);
  if (d) {
    while ((dir = readdir(d)) != NULL) {
      if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0 && strcmp(dir->d_name, ".git") != 0 &&
          strcmp(dir->d_name, ".idea") != 0) {
        char abs_path[PATH_MAX];
        snprintf(abs_path, sizeof(abs_path), "%s/%s", folder->path, dir->d_name);
        if (dir->d_type == DT_DIR) {
          // Dir
          // reallocate for one new element.
          folder->folder_count++;
          ExplorerFolder* new_folders = realloc(folder->folders, sizeof(ExplorerFolder) * folder->folder_count);
          if (new_folders == NULL) {
            folder->folder_count--;
            continue;
          }
          folder->folders = new_folders;
          // insert in alphabet order
          int i = 0;
          for (; i < folder->folder_count - 1; i++) {
            if (strcicmp(dir->d_name, basename(folder->folders[i].path)) < 0) {
              break;
            }
          }
          memmove(folder->folders + i + 1, folder->folders + i,
                  (folder->folder_count - i - 1) * sizeof(ExplorerFolder));
          initFolder(abs_path, folder->folders + i);
        }
        else {
          // file
          folder->file_count++;
          folder->files = realloc(folder->files, sizeof(ExplorerFile) * folder->file_count);
          // insert in alphabet order
          int i = 0;
          for (; i < folder->file_count - 1; i++) {
            if (strcicmp(dir->d_name, basename(folder->files[i].path)) < 0) {
              break;
            }
          }
          memmove(folder->files + i + 1, folder->files + i, (folder->file_count - i - 1) * sizeof(ExplorerFile));
          realpath(abs_path, folder->files[i].path);
        }
      }
    }
    closedir(d);
  }
  folder->discovered = true;
}


void destroyFolder(ExplorerFolder* folder) {
  if (folder == NULL) {
    return;
  }

  free(folder->files);
  for (int i = 0; i < folder->folder_count; i++) {
    destroyFolder(folder->folders + i);
  }
  free(folder->folders);
}

void reloadFolder(ExplorerFolder* folder) {
  if (folder == NULL) {
    return;
  }

  free(folder->files);
  for (int i = 0; i < folder->folder_count; i++) {
    destroyFolder(folder->folders + i);
  }
  free(folder->folders);

  folder->folders = NULL;
  folder->files = NULL;
  folder->folder_count = 0;
  folder->file_count = 0;
  folder->discovered = false;

  if (folder->open) {
    discoverFolder(folder);
  }
}

