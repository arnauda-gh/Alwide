#include "config.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../utils/logger.h"

bool configExist() {
  char path[PATH_MAX];
  const char* home = getenv("HOME");
  if (!home) {
    return false;
  }
  snprintf(path, PATH_MAX, "%s/%s", home, CONFIG_PATH);

  FILE* f = fopen(path, "r");
  if (f == NULL) {
    return false;
  }
  fclose(f);
  return true;
}

void touchConfig() {
  char command[PATH_MAX + 100];
  const char* home = getenv("HOME");
  if (!home) {
    return;
  }
  snprintf(command, sizeof(command), "mkdir -p \"%s/%s\"", home, CONFIG_FOLDER);
  system(command);
  snprintf(command, sizeof(command), "touch \"%s/%s\"", home, CONFIG_PATH);
  system(command);
}

cJSON* loadConfig() {
  const char* home = getenv("HOME");
  const char* assets_env = getenv("ALWIDE_ASSETS_PATH");
  char path[PATH_MAX];

  if (!home) {
    return NULL;
  }

  if (configExist() == false) {
    touchConfig();

    snprintf(path, PATH_MAX, "%s/%s", home, CONFIG_PATH);
    notifyUser(NULL, LOG_INFO, "Generating default config to %s", path);

    FILE* f = fopen(path, "w");
    if (f == NULL) {
      notifyUser(NULL, LOG_ERROR, "ERROR opening config file.");
      return NULL;
    }

    // Determine the best default path for assets by checking for languages-features.json
    char default_assets_path[PATH_MAX];
    char check_path[PATH_MAX];

    if (assets_env) {
      strncpy(default_assets_path, assets_env, PATH_MAX - 1);
    }
    else {
      // 1. Check local assets (for developers)
      if (access("assets/languages-features.json", F_OK) == 0) {
        getcwd(default_assets_path, PATH_MAX);
        strncat(default_assets_path, "/assets", PATH_MAX - strlen(default_assets_path) - 1);
      }
      // 2. Check user config
      else {
        snprintf(check_path, PATH_MAX, "%s/.config/alwide/languages-features.json", home);
        if (access(check_path, F_OK) == 0) {
          strncpy(default_assets_path, "~/.config/alwide", PATH_MAX - 1);
        }
#ifdef DATADIR
        // 3. Check system-wide assets
        else {
          snprintf(check_path, PATH_MAX, "%s/languages-features.json", DATADIR);
          if (access(check_path, F_OK) == 0) {
            strncpy(default_assets_path, DATADIR, PATH_MAX - 1);
          }
          else {
            strncpy(default_assets_path, "~/.config/alwide", PATH_MAX - 1);
          }
        }
#else
        else {
          strncpy(default_assets_path, "~/.config/alwide", PATH_MAX - 1);
        }
#endif
      }
    }
    default_assets_path[PATH_MAX - 1] = '\0';

    fprintf(f, "{\n    \"default_path\": \"%s\"\n}\n", default_assets_path);
    fclose(f);
  }

  snprintf(path, PATH_MAX, "%s/%s", home, CONFIG_PATH);

  FILE* f = fopen(path, "rb");
  if (f == NULL) {
    return NULL;
  }

  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET); /* same as rewind(f); */

  if (fsize < 0) {
    fclose(f);
    return NULL;
  }

  char* file_content = malloc(fsize + 1);
  if (!file_content) {
    fclose(f);
    return NULL;
  }

  size_t bytes_read = fread(file_content, 1, fsize, f);
  fclose(f);

  file_content[bytes_read] = 0;

  cJSON* json = cJSON_Parse(file_content);

  free(file_content);
  return json;
}
