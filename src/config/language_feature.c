#include "language_feature.h"

#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../environnement/global_variables.h"
#include "../utils/tools.h"
#include "../utils/logger.h"
#include "config.h"

LF_LanguageFeature default_feature = {
  .id = "plain",
  .label = "Plain Text",
  .detect = {.extensions = NULL,
             .extensions_count = 0,
             .filenames = NULL,
             .filenames_count = 0,
             .shebangs = NULL,
             .shebangs_count = 0},
  .comments = {.line = "//", .block_start = "/*", .block_end = "*/"}, // Reasonable defaults
  .tabulation = {.size = 2, .use_space = true},
  .pairs_count = 0,
  .lsp = {.exe = "", .arguments = ""}};

void initLanguageFeatureList(LF_LanguageFeatureList* list) {
  list->list = NULL;
  list->size = 0;
}


/**
 * Helper to parse a JSON array of strings into a dynamically allocated contiguous array of fixed-size strings.
 */
static void* parseStringArrayDynamic(cJSON* array, int* count, int item_len) {
  if (!cJSON_IsArray(array)) {
    *count = 0;
    return NULL;
  }
  int size = cJSON_GetArraySize(array);
  *count = size;
  if (size == 0) {
    return NULL;
  }
  void* dest = malloc(size * item_len);
  if (!dest) {
    *count = 0;
    return NULL;
  }
  for (int i = 0; i < size; i++) {
    cJSON* item = cJSON_GetArrayItem(array, i);
    const char* val = cJSON_GetStringValue(item);
    char* target = (char*)dest + (i * item_len);
    if (val) {
      strncpy(target, val, item_len - 1);
      target[item_len - 1] = '\0';
    }
    else {
      target[0] = '\0';
    }
  }
  return dest;
}

void LF_loadLanguageFeatures() {
  const char* home = getenv("HOME");
  if (!home) {
    return;
  }
  initLanguageFeatureList(&language_features);

  // Fetching the folder where to load theme and queries.
  char* load_path_raw = cJSON_GetStringValue(cJSON_GetObjectItem(config, "default_path"));
  char load_path[PATH_MAX];
  resolvePath(load_path, sizeof(load_path), load_path_raw);

  char path[PATH_MAX];
  // Loading features
  snprintf(path, sizeof(path), "%s/languages-features.json", load_path);

  long length;
  char* content = loadFullFile(path, &length);
  if (!content) {
    notifyUser(NULL, LOG_ERROR, "Failed to load languages-features.json from %s.", path);
    return;
  }

  cJSON* json = cJSON_Parse(content);
  free(content);
  if (!json) {
    return;
  }

  language_features.size = cJSON_GetArraySize(json);
  language_features.list = malloc(sizeof(LF_LanguageFeature) * language_features.size);

  for (int i = 0; i < language_features.size; i++) {
    cJSON* lang_json = cJSON_GetArrayItem(json, i);
    LF_LanguageFeature* lang = &language_features.list[i];

    const char* id_val = cJSON_GetStringValue(cJSON_GetObjectItem(lang_json, "id"));
    if (id_val) {
      strncpy(lang->id, id_val, sizeof(lang->id) - 1);
      lang->id[sizeof(lang->id) - 1] = '\0';
    }
    else {
      lang->id[0] = '\0';
    }

    const char* label_val = cJSON_GetStringValue(cJSON_GetObjectItem(lang_json, "label"));
    if (label_val) {
      strncpy(lang->label, label_val, sizeof(lang->label) - 1);
      lang->label[sizeof(lang->label) - 1] = '\0';
    }
    else {
      lang->label[0] = '\0';
    }

    cJSON* detect = cJSON_GetObjectItem(lang_json, "detect");
    lang->detect.extensions = parseStringArrayDynamic(cJSON_GetObjectItem(detect, "extensions"),
                                                      &lang->detect.extensions_count, LF_EXTENSION_MAX_LENGTH);
    lang->detect.filenames = parseStringArrayDynamic(cJSON_GetObjectItem(detect, "file-name"),
                                                     &lang->detect.filenames_count, LF_FILENAME_MAX_LENGTH);
    lang->detect.shebangs = parseStringArrayDynamic(cJSON_GetObjectItem(detect, "shebang"),
                                                    &lang->detect.shebangs_count, LF_SHEBANG_MAX_LENGTH);

    cJSON* comments = cJSON_GetObjectItem(lang_json, "comments");
    const char* line_val = cJSON_GetStringValue(cJSON_GetObjectItem(comments, "line"));
    if (line_val) {
      strncpy(lang->comments.line, line_val, sizeof(lang->comments.line) - 1);
      lang->comments.line[sizeof(lang->comments.line) - 1] = '\0';
    }
    else {
      lang->comments.line[0] = '\0';
    }

    cJSON* block = cJSON_GetObjectItem(comments, "block");
    if (cJSON_IsArray(block) && cJSON_GetArraySize(block) == 2) {
      const char* bstart = cJSON_GetStringValue(cJSON_GetArrayItem(block, 0));
      const char* bend = cJSON_GetStringValue(cJSON_GetArrayItem(block, 1));
      if (bstart) {
        strncpy(lang->comments.block_start, bstart, sizeof(lang->comments.block_start) - 1);
        lang->comments.block_start[sizeof(lang->comments.block_start) - 1] = '\0';
      }
      else {
        lang->comments.block_start[0] = '\0';
      }
      if (bend) {
        strncpy(lang->comments.block_end, bend, sizeof(lang->comments.block_end) - 1);
        lang->comments.block_end[sizeof(lang->comments.block_end) - 1] = '\0';
      }
      else {
        lang->comments.block_end[0] = '\0';
      }
    }
    else {
      lang->comments.block_start[0] = '\0';
      lang->comments.block_end[0] = '\0';
    }

    cJSON* tab = cJSON_GetObjectItem(lang_json, "tabulation");
    lang->tabulation.size = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(tab, "size"));
    lang->tabulation.use_space = cJSON_IsTrue(cJSON_GetObjectItem(tab, "space"));

    cJSON* pairs_arr = cJSON_GetObjectItem(lang_json, "pairs");
    int pairs_size = cJSON_GetArraySize(pairs_arr);
    lang->pairs_count = pairs_size;
    if (pairs_size > 0) {
      lang->pairs = malloc(sizeof(LF_Pair) * pairs_size);
      if (lang->pairs) {
        for (int j = 0; j < pairs_size; j++) {
          cJSON* pair = cJSON_GetArrayItem(pairs_arr, j);
          const char* open_val = cJSON_GetStringValue(cJSON_GetArrayItem(pair, 0));
          const char* close_val = cJSON_GetStringValue(cJSON_GetArrayItem(pair, 1));

          if (open_val) {
            strncpy(lang->pairs[j].open, open_val, sizeof(lang->pairs[j].open) - 1);
            lang->pairs[j].open[sizeof(lang->pairs[j].open) - 1] = '\0';
          }
          else {
            lang->pairs[j].open[0] = '\0';
          }

          if (close_val) {
            strncpy(lang->pairs[j].close, close_val, sizeof(lang->pairs[j].close) - 1);
            lang->pairs[j].close[sizeof(lang->pairs[j].close) - 1] = '\0';
          }
          else {
            lang->pairs[j].close[0] = '\0';
          }
        }
      }
    }
    else {
      lang->pairs = NULL;
    }

    cJSON* lsp = cJSON_GetObjectItem(lang_json, "lsp");
    const char* exe_val = cJSON_GetStringValue(cJSON_GetObjectItem(lsp, "exe"));
    if (exe_val) {
      strncpy(lang->lsp.exe, exe_val, sizeof(lang->lsp.exe) - 1);
      lang->lsp.exe[sizeof(lang->lsp.exe) - 1] = '\0';
    }
    else {
      lang->lsp.exe[0] = '\0';
    }

    cJSON* args_arr = cJSON_GetObjectItem(lsp, "arguments");
    int args_count = cJSON_GetArraySize(args_arr);
    if (args_count > 0) {
      char args_buf[LANG_ID_LENGTH] = {0};
      for (int j = 0; j < args_count; j++) {
        const char* arg_val = cJSON_GetStringValue(cJSON_GetArrayItem(args_arr, j));
        if (arg_val) {
          strcat(args_buf, arg_val);
          if (j < args_count - 1) {
            strcat(args_buf, " ");
          }
        }
      }
      strncpy(lang->lsp.arguments, args_buf, sizeof(lang->lsp.arguments) - 1);
      lang->lsp.arguments[sizeof(lang->lsp.arguments) - 1] = '\0';
    }
    else {
      lang->lsp.arguments[0] = '\0';
    }
  }

  cJSON_Delete(json);
}

LF_LanguageFeature* LF_getFeatureById(const char* id) {
  if (id) {
    for (int i = 0; i < language_features.size; i++) {
      if (strcmp(language_features.list[i].id, id) == 0) {
        return &language_features.list[i];
      }
    }
  }
  return &default_feature;
}

LF_LanguageFeature* LF_getFeatureForFile(const char* filepath) {
  if (!filepath) {
    return LF_getFeatureById("plain");
  }

  char first_line[256] = {0};
  FILE* f = fopen(filepath, "r");
  if (f) {
    if (fgets(first_line, sizeof(first_line), f) == NULL) {
      first_line[0] = '\0';
    }
    fclose(f);
  }

  const char* detected = LF_detectLanguage(filepath, first_line);
  return LF_getFeatureById(detected);
}

LF_Tabulation* LF_tab(LF_LanguageFeature* ft) { return &ft->tabulation; }

int LF_tab_size(LF_LanguageFeature* ft) { return ft->tabulation.size; }

bool LF_tab_use_space(LF_LanguageFeature* ft) { return ft->tabulation.use_space; }

const char* LF_id(LF_LanguageFeature* ft) { return ft->id; }

const char* LF_label(LF_LanguageFeature* ft) { return ft->label; }

const char* LF_comment_line(LF_LanguageFeature* ft) { return ft->comments.line; }

const char* LF_comment_block_start(LF_LanguageFeature* ft) { return ft->comments.block_start; }

const char* LF_comment_block_end(LF_LanguageFeature* ft) { return ft->comments.block_end; }

int LF_pairs_count(LF_LanguageFeature* ft) { return ft->pairs_count; }

LF_Pair* LF_pair(LF_LanguageFeature* ft, int index) {
  if (index < 0 || index >= ft->pairs_count) {
    return NULL;
  }
  return &ft->pairs[index];
}

const char* LF_lsp_exe(LF_LanguageFeature* ft) { return ft->lsp.exe; }

const char* LF_lsp_args(LF_LanguageFeature* ft) { return ft->lsp.arguments; }

const char* LF_detectLanguage(const char* filepath, const char* first_line) {
  if (!filepath) {
    return NULL;
  }

  char* bname = strdup(filepath);
  char* filename = basename(bname);
  char* dot = strrchr(filename, '.');

  const char* found_id = NULL;

  for (int i = 0; i < language_features.size; i++) {
    LF_LanguageFeature* lang = &language_features.list[i];

    // 1. Match filenames
    for (int j = 0; j < lang->detect.filenames_count; j++) {
      if (strcmp(filename, lang->detect.filenames[j]) == 0) {
        found_id = lang->id;
        goto cleanup;
      }
    }

    // 2. Match extensions
    if (dot) {
      for (int j = 0; j < lang->detect.extensions_count; j++) {
        if (strcmp(dot, lang->detect.extensions[j]) == 0) {
          found_id = lang->id;
          goto cleanup;
        }
      }
    }

    // 3. Match shebangs
    if (first_line && first_line[0] == '#' && first_line[1] == '!') {
      for (int j = 0; j < lang->detect.shebangs_count; j++) {
        if (strstr(first_line, lang->detect.shebangs[j]) != NULL) {
          found_id = lang->id;
          goto cleanup;
        }
      }
    }
  }

cleanup:
  free(bname);
  return found_id;
}

void destroyLanguageFeatureList(LF_LanguageFeatureList* list) {
  if (!list || !list->list) {
    return;
  }
  for (int i = 0; i < list->size; i++) {
    free(list->list[i].detect.extensions);
    free(list->list[i].detect.filenames);
    free(list->list[i].detect.shebangs);
    free(list->list[i].pairs);
  }
  free(list->list);
  list->list = NULL;
  list->size = 0;
}
