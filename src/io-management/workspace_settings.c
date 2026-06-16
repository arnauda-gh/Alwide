#include "workspace_settings.h"

#include <assert.h>

#include "../config/config.h"

#include <string.h>
#include <sys/ttydefaults.h>
#include "../environnement/constants.h"


void getWorkspaceSettingsForCurrentDir(WorkspaceSettings* settings, FileContainer* files, int file_count,
                                       int current_file, bool showing_opened_file_window,
                                       bool showing_file_explorer_window, int file_explorer_size) {
  settings->file_count = file_count;
  settings->current_opened_file = current_file;
  settings->files = malloc(file_count * sizeof(char*));
  for (int i = 0; i < file_count; i++) {
    int size = strlen(files[i].io_file.path_abs) + 1;
    settings->files[i] = malloc(size * sizeof(char));
    memcpy(settings->files[i], files[i].io_file.path_abs, size);
    settings->files[i][size - 1] = '\0';
  }
  settings->file_explorer_size = file_explorer_size;
  settings->showing_file_explorer_window = showing_file_explorer_window;
  settings->showing_opened_file_window = showing_opened_file_window;

  settings->nav_back_size = 0;
  settings->nav_back_items = NULL;
  settings->nav_forward_size = 0;
  settings->nav_forward_items = NULL;
}

void destroyWorkspaceSettings(WorkspaceSettings* settings) {
  for (int i = 0; i < settings->file_count; i++) {
    free(settings->files[i]);
  }
  free(settings->files);

  if (settings->nav_back_items != NULL) {
    free(settings->nav_back_items);
  }
  if (settings->nav_forward_items != NULL) {
    free(settings->nav_forward_items);
  }
}

void touchDirSettingsFolder() {
  const char* home = getenv("HOME");
  if (!home) {
    return;
  }
  char command[PATH_MAX + 100];
  snprintf(command, sizeof(command), "mkdir -p \"%s/%s\"", home, CONFIG_FOLDER);
  system(command);
  snprintf(command, sizeof(command), "mkdir -p \"%s/%s/%s\"", home, CONFIG_FOLDER, FOLDER_DIR_SETTINGS_NAME);
  system(command);
}

void saveWorkspaceSettings(char* dir_path, WorkspaceSettings* settings) {
  touchDirSettingsFolder();

  char abs_dir_path[PATH_MAX];
  realpath(dir_path, abs_dir_path);

  unsigned long long hash_dir_path = hashFileName(abs_dir_path);

  const char* home = getenv("HOME");
  if (!home) {
    return;
  }

  snprintf(abs_dir_path, sizeof(abs_dir_path), "%s/%s/%s/%llu", home, CONFIG_FOLDER, FOLDER_DIR_SETTINGS_NAME,
           hash_dir_path);

  FILE* f = fopen(abs_dir_path, "w");
  if (f == NULL) {
    fprintf(stderr, "Unable to save dir settings.\r\n");
    return;
  }

  cJSON* json_settings = WorkspaceSettingsToJSON(settings);
  char* json_text = cJSON_Print(json_settings);

  fprintf(f, "%s", json_text);

  cJSON_Delete(json_settings);

  free(json_text);
  fclose(f);
}

bool loadWorkspaceSettings(char* dir_path, WorkspaceSettings* settings) {
  char abs_dir_path[PATH_MAX];
  realpath(dir_path, abs_dir_path);

  unsigned long long hash_dir_path = hashFileName(abs_dir_path);

  const char* home = getenv("HOME");
  if (!home) {
    return false;
  }

  snprintf(abs_dir_path, sizeof(abs_dir_path), "%s/%s/%s/%llu", home, CONFIG_FOLDER, FOLDER_DIR_SETTINGS_NAME,
           hash_dir_path);

  FILE* f = fopen(abs_dir_path, "r");
  if (f == NULL) {
    // init default values.
    settings->file_count = 0;
    settings->files = NULL;
    settings->showing_file_explorer_window = false;
    settings->showing_opened_file_window = false;
    settings->file_explorer_size = 0;
    settings->nav_back_size = 0;
    settings->nav_back_items = NULL;
    settings->nav_forward_size = 0;
    settings->nav_forward_items = NULL;
    // fprintf(stderr, "Unable to load dir settings.\r\n");
    return false;
  }

  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET); /* same as rewind(f); */

  char* file_content = malloc(fsize + 1);
  fread(file_content, fsize, 1, f);
  fclose(f);

  file_content[fsize] = 0;

  cJSON* json_settings = cJSON_Parse(file_content);

  free(file_content);

  JSONToWorkspaceSettings(settings, json_settings);

  cJSON_Delete(json_settings);

  return true;
}

cJSON* WorkspaceSettingsToJSON(WorkspaceSettings* settings) {
  cJSON* json_settings = cJSON_CreateObject();
  cJSON_AddNumberToObject(json_settings, "file_count", settings->file_count);
  cJSON_AddNumberToObject(json_settings, "current_opened_file", settings->current_opened_file);
  cJSON* file_array = cJSON_AddArrayToObject(json_settings, "files");
  for (int i = 0; i < settings->file_count; i++) {
    cJSON_AddItemToArray(file_array, cJSON_CreateString(settings->files[i]));
  }
  cJSON_AddBoolToObject(json_settings, "showing_opened_file_window", settings->showing_opened_file_window);
  cJSON_AddBoolToObject(json_settings, "showing_file_explorer_window", settings->showing_file_explorer_window);
  cJSON_AddNumberToObject(json_settings, "file_explorer_size", settings->file_explorer_size);

  cJSON* nav_back_array = cJSON_AddArrayToObject(json_settings, "nav_back");
  for (int i = 0; i < settings->nav_back_size; i++) {
    cJSON* loc_json = cJSON_CreateObject();
    cJSON_AddStringToObject(loc_json, "file_path", settings->nav_back_items[i].file_path);
    cJSON_AddNumberToObject(loc_json, "row", settings->nav_back_items[i].row);
    cJSON_AddNumberToObject(loc_json, "column", settings->nav_back_items[i].column);
    cJSON_AddNumberToObject(loc_json, "screen_x", settings->nav_back_items[i].screen_x);
    cJSON_AddNumberToObject(loc_json, "screen_y", settings->nav_back_items[i].screen_y);
    cJSON_AddItemToArray(nav_back_array, loc_json);
  }

  cJSON* nav_forward_array = cJSON_AddArrayToObject(json_settings, "nav_forward");
  for (int i = 0; i < settings->nav_forward_size; i++) {
    cJSON* loc_json = cJSON_CreateObject();
    cJSON_AddStringToObject(loc_json, "file_path", settings->nav_forward_items[i].file_path);
    cJSON_AddNumberToObject(loc_json, "row", settings->nav_forward_items[i].row);
    cJSON_AddNumberToObject(loc_json, "column", settings->nav_forward_items[i].column);
    cJSON_AddNumberToObject(loc_json, "screen_x", settings->nav_forward_items[i].screen_x);
    cJSON_AddNumberToObject(loc_json, "screen_y", settings->nav_forward_items[i].screen_y);
    cJSON_AddItemToArray(nav_forward_array, loc_json);
  }

  return json_settings;
}

void JSONToWorkspaceSettings(WorkspaceSettings* settings, cJSON* json) {
  settings->file_count = cJSON_GetNumberValue(cJSON_GetObjectItem(json, "file_count"));
  settings->current_opened_file = cJSON_GetNumberValue(cJSON_GetObjectItem(json, "current_opened_file"));
  cJSON* file_array = cJSON_GetObjectItem(json, "files");
  settings->files = malloc(sizeof(char*) * settings->file_count);
  for (int i = 0; i < settings->file_count; i++) {
    char* value = cJSON_GetStringValue(cJSON_GetArrayItem(file_array, i));

    int size = strlen(value) + 1;
    settings->files[i] = malloc(size * sizeof(char));
    memcpy(settings->files[i], value, size);
    settings->files[i][size - 1] = '\0';
  }

  settings->showing_opened_file_window = cJSON_IsTrue(cJSON_GetObjectItem(json, "showing_opened_file_window"));
  settings->showing_file_explorer_window = cJSON_IsTrue(cJSON_GetObjectItem(json, "showing_file_explorer_window"));
  settings->file_explorer_size = cJSON_GetNumberValue(cJSON_GetObjectItem(json, "file_explorer_size"));

  cJSON* nav_back_array = cJSON_GetObjectItem(json, "nav_back");
  if (cJSON_IsArray(nav_back_array)) {
    settings->nav_back_size = cJSON_GetArraySize(nav_back_array);
    if (settings->nav_back_size > 0) {
      settings->nav_back_items = malloc(settings->nav_back_size * sizeof(NavigationLocation));
      for (int i = 0; i < settings->nav_back_size; i++) {
        cJSON* item = cJSON_GetArrayItem(nav_back_array, i);
        cJSON* file_path = cJSON_GetObjectItem(item, "file_path");
        cJSON* row = cJSON_GetObjectItem(item, "row");
        cJSON* column = cJSON_GetObjectItem(item, "column");
        cJSON* screen_x = cJSON_GetObjectItem(item, "screen_x");
        cJSON* screen_y = cJSON_GetObjectItem(item, "screen_y");

        if (cJSON_IsString(file_path)) {
          strncpy(settings->nav_back_items[i].file_path, cJSON_GetStringValue(file_path),
                  sizeof(settings->nav_back_items[i].file_path) - 1);
          settings->nav_back_items[i].file_path[sizeof(settings->nav_back_items[i].file_path) - 1] = '\0';
        }
        else {
          settings->nav_back_items[i].file_path[0] = '\0';
        }
        settings->nav_back_items[i].row = cJSON_IsNumber(row) ? (int)cJSON_GetNumberValue(row) : 0;
        settings->nav_back_items[i].column = cJSON_IsNumber(column) ? (int)cJSON_GetNumberValue(column) : 0;
        settings->nav_back_items[i].screen_x = cJSON_IsNumber(screen_x) ? (int)cJSON_GetNumberValue(screen_x) : 0;
        settings->nav_back_items[i].screen_y = cJSON_IsNumber(screen_y) ? (int)cJSON_GetNumberValue(screen_y) : 0;
      }
    }
    else {
      settings->nav_back_items = NULL;
    }
  }
  else {
    settings->nav_back_size = 0;
    settings->nav_back_items = NULL;
  }

  cJSON* nav_forward_array = cJSON_GetObjectItem(json, "nav_forward");
  if (cJSON_IsArray(nav_forward_array)) {
    settings->nav_forward_size = cJSON_GetArraySize(nav_forward_array);
    if (settings->nav_forward_size > 0) {
      settings->nav_forward_items = malloc(settings->nav_forward_size * sizeof(NavigationLocation));
      for (int i = 0; i < settings->nav_forward_size; i++) {
        cJSON* item = cJSON_GetArrayItem(nav_forward_array, i);
        cJSON* file_path = cJSON_GetObjectItem(item, "file_path");
        cJSON* row = cJSON_GetObjectItem(item, "row");
        cJSON* column = cJSON_GetObjectItem(item, "column");
        cJSON* screen_x = cJSON_GetObjectItem(item, "screen_x");
        cJSON* screen_y = cJSON_GetObjectItem(item, "screen_y");

        if (cJSON_IsString(file_path)) {
          strncpy(settings->nav_forward_items[i].file_path, cJSON_GetStringValue(file_path),
                  sizeof(settings->nav_forward_items[i].file_path) - 1);
          settings->nav_forward_items[i].file_path[sizeof(settings->nav_forward_items[i].file_path) - 1] = '\0';
        }
        else {
          settings->nav_forward_items[i].file_path[0] = '\0';
        }
        settings->nav_forward_items[i].row = cJSON_IsNumber(row) ? (int)cJSON_GetNumberValue(row) : 0;
        settings->nav_forward_items[i].column = cJSON_IsNumber(column) ? (int)cJSON_GetNumberValue(column) : 0;
        settings->nav_forward_items[i].screen_x = cJSON_IsNumber(screen_x) ? (int)cJSON_GetNumberValue(screen_x) : 0;
        settings->nav_forward_items[i].screen_y = cJSON_IsNumber(screen_y) ? (int)cJSON_GetNumberValue(screen_y) : 0;
      }
    }
    else {
      settings->nav_forward_items = NULL;
    }
  }
  else {
    settings->nav_forward_size = 0;
    settings->nav_forward_items = NULL;
  }
}

void setupWorkspace(WorkspaceSettings* loaded_settings, int* file_count, char*** file_names, gui_Context* gui_context,
                    int* current_file_index) {
  loaded_settings->is_used = false;
  if (*file_count == 1 || *file_count == 0) {
    char* dir_name = *file_count == 0 ? getenv("PWD") : (*file_names)[0];

    if (isDir(dir_name)) {
      loaded_settings->dir_path = dir_name;
      loaded_settings->is_used = true;

      bool settings_exist = loadWorkspaceSettings(dir_name, loaded_settings);

      // consume dir name
      if (*file_count == 1) {
        (*file_count)--;
        (*file_names)++;
      }
      assert((*file_count) == 0);

      if (settings_exist) {
        // Setup opened files.
        *file_count = loaded_settings->file_count;
        *file_names = loaded_settings->files;


        // --- UI State ---

        // Current showed file.
        *current_file_index = loaded_settings->current_opened_file;

        // File Opened Window state.
        if (loaded_settings->showing_opened_file_window == true) {
          gui_context->ofw_context.ofw_height = OPENED_FILE_WINDOW_HEIGHT;
        }
        else {
          gui_context->ofw_context.ofw_height = 0;
        }

        // File Explorer Window state.
        if (loaded_settings->showing_file_explorer_window == true) {
          ungetch(CTRL('e'));
        }
      }
    }
  }
}
