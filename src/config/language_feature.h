#ifndef LANGUAGE_FEATURE_H
#define LANGUAGE_FEATURE_H

#include <stdbool.h>
#include "../environnement/constants.h"


#define LF_EXTENSION_MAX_LENGTH 16
#define LF_FILENAME_MAX_LENGTH  100
#define LF_SHEBANG_MAX_LENGTH   100

#define LF_PAIR_MAX_LENGTH     8
#define LF_COMMENTS_MAX_LENGTH 16

/**
 * Detection rules for a language.
 */
typedef struct {
  char (*extensions)[LF_EXTENSION_MAX_LENGTH];
  int extensions_count;
  char (*filenames)[LF_FILENAME_MAX_LENGTH];
  int filenames_count;
  char (*shebangs)[LF_SHEBANG_MAX_LENGTH];
  int shebangs_count;
} LF_Detect;

/**
 * Comment styles for a language.
 */
typedef struct {
  char line[LF_COMMENTS_MAX_LENGTH];
  char block_start[LF_COMMENTS_MAX_LENGTH];
  char block_end[LF_COMMENTS_MAX_LENGTH];
} LF_Comments;

/**
 * Tabulation settings.
 */
typedef struct {
  int size;
  bool use_space;
} LF_Tabulation;

/**
 * Auto-completion pairs (e.g., (), {}, "").
 */
typedef struct {
  char open[LF_PAIR_MAX_LENGTH];
  char close[LF_PAIR_MAX_LENGTH];
} LF_Pair;

/**
 * LSP server configuration.
 */
typedef struct {
  char exe[LANG_ID_LENGTH];
  char arguments[LANG_ID_LENGTH];
} LF_LSP;

/**
 * Full language feature definition.
 */
typedef struct {
  char id[LANG_ID_LENGTH];
  char label[LANG_ID_LENGTH];
  LF_Detect detect;
  LF_Comments comments;
  LF_Tabulation tabulation;
  LF_Pair* pairs;
  int pairs_count;
  LF_LSP lsp;
} LF_LanguageFeature;

typedef struct {
  LF_LanguageFeature* list;
  int size;
} LF_LanguageFeatureList;

extern LF_LanguageFeature default_feature;

void initLanguageFeatureList(LF_LanguageFeatureList* list);
void destroyLanguageFeatureList(LF_LanguageFeatureList* list);

/**
 * Loads all language features from the JSON configuration file.
 */
void LF_loadLanguageFeatures();

/**
 * Detects the language ID for a given file path and optional first line content (shebang).
 */
const char* LF_detectLanguage(const char* filepath, const char* first_line);

/**
 * Retrieves the language feature structure for a given language ID.
 */
LF_LanguageFeature* LF_getFeatureById(const char* id);

/**
 * Detects and retrieves the language feature structure for a given file path.
 */
LF_LanguageFeature* LF_getFeatureForFile(const char* filepath);

LF_Tabulation* LF_tab(LF_LanguageFeature* ft);
int LF_tab_size(LF_LanguageFeature* ft);
bool LF_tab_use_space(LF_LanguageFeature* ft);

const char* LF_id(LF_LanguageFeature* ft);
const char* LF_label(LF_LanguageFeature* ft);

const char* LF_comment_line(LF_LanguageFeature* ft);
const char* LF_comment_block_start(LF_LanguageFeature* ft);
const char* LF_comment_block_end(LF_LanguageFeature* ft);

int LF_pairs_count(LF_LanguageFeature* ft);
LF_Pair* LF_pair(LF_LanguageFeature* ft, int index);

const char* LF_lsp_exe(LF_LanguageFeature* ft);
const char* LF_lsp_args(LF_LanguageFeature* ft);

#endif // LANGUAGE_FEATURE_H
