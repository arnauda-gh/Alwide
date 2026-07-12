#include "tree_manager.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tree_sitter/api.h>
#include "../../environnement/constants.h"
#include "../../environnement/global_variables.h"
#include "../../terminal/highlight.h"
#include "../../utils/tools.h"
#include "../../utils/logger.h"
#include "../tree-sitter/tree_query.h"


void initParserList(ParserList* list) {
  list->size = 0;
  list->list = NULL;
}

void addParserToParserList(ParserList* list, ParserContainer new_parser) {
  list->size++;
  list->list = realloc(list->list, list->size * sizeof(ParserContainer));
  list->list[list->size - 1] = new_parser;
}

void destroyParserList(ParserList* list) {
  for (int i = 0; i < list->size; i++) {
    ts_parser_delete(list->list[i].parser);
    ts_language_delete(list->list[i].lang);
    ts_query_delete(list->list[i].queries);
    destroyRegexMap(&list->list[i].regex_map);
    destroyThemeList(&list->list[i].theme_list);
  }
  free(list->list);
  list->list = NULL;
  list->size = 0;
}

ParserContainer* getParserForLanguage(ParserList* list, char* language) {
  for (int i = 0; i < list->size; i++) {
    if (strcmp(list->list[i].lang_id, language) == 0) {
      return list->list + i;
    }
  }

  ParserContainer new_parser;
  bool result = loadNewParser(&new_parser, language);
  if (result == false) {
    return NULL;
  }

  addParserToParserList(list, new_parser);

  return list->list + list->size - 1;
}

TSQuery* loadQueries(const ParserContainer* container, char path[PATH_MAX], uint32_t* error_offset,
                     TSQueryError* error_type) {
  long length;
  char* source = loadFullFile(path, &length);
  TSQuery* queries = ts_query_new(container->lang, source, length, error_offset, error_type);
  free(source);
  return queries;
}

void getTSLanguageFromString(const TSLanguage** lang, char* language) {
  // ADD_NEW_LANGUAGE
  if (strcmp(language, "c") == 0) {
    *lang = tree_sitter_c();
  }
  else if (strcmp(language, "python") == 0) {
    *lang = tree_sitter_python();
  }
  else if (strcmp(language, "markdown") == 0) {
    *lang = tree_sitter_markdown();
  }
  else if (strcmp(language, "markdown_inline") == 0) {
    *lang = tree_sitter_markdown_inline();
  }
  else if (strcmp(language, "java") == 0) {
    *lang = tree_sitter_java();
  }
  else if (strcmp(language, "cpp") == 0) {
    *lang = tree_sitter_cpp();
  }
  else if (strcmp(language, "c-sharp") == 0) {
    *lang = tree_sitter_c_sharp();
  }
  else if (strcmp(language, "make") == 0) {
    *lang = tree_sitter_make();
  }
  else if (strcmp(language, "css") == 0) {
    *lang = tree_sitter_css();
  }
  else if (strcmp(language, "dart") == 0) {
    *lang = tree_sitter_dart();
  }
  else if (strcmp(language, "go") == 0) {
    *lang = tree_sitter_go();
  }
  else if (strcmp(language, "javascript") == 0) {
    *lang = tree_sitter_javascript();
  }
  else if (strcmp(language, "json") == 0) {
    *lang = tree_sitter_json();
  }
  else if (strcmp(language, "bash") == 0) {
    *lang = tree_sitter_bash();
  }
  else if (strcmp(language, "query") == 0) {
    *lang = tree_sitter_query();
  }
  else if (strcmp(language, "vhdl") == 0) {
    *lang = tree_sitter_vhdl();
  }
  else if (strcmp(language, "lua") == 0) {
    *lang = tree_sitter_lua();
  }
  else if (strcmp(language, "asm") == 0) {
    *lang = tree_sitter_asm();
  }
  else if (strcmp(language, "html") == 0) {
    *lang = tree_sitter_html();
  }
  else if (strcmp(language, "latex") == 0) {
    *lang = tree_sitter_latex();
  }
  else if (strcmp(language, "ini") == 0) {
    *lang = tree_sitter_ini();
  }
  else if (strcmp(language, "yaml") == 0 || strcmp(language, "yml") == 0) {
    *lang = tree_sitter_yaml();
  }
  else {
    *lang = NULL;
  }
}


bool loadNewParser(ParserContainer* container, char* language) {
  // Set file name
  snprintf(container->lang_id, sizeof(container->lang_id), "%s", language);

  // Getting TSLanguage
  getTSLanguageFromString(&container->lang, language);

  if (container->lang == NULL) {
    if (strcmp("plain", container->lang_id) != 0) {
      notifyUser(NULL, LOG_WARNING, "Language : '%s' tree-sitter wasn't implemented !", container->lang_id);
    }
    return false;
  }

  // Fetching the folder where to load theme and queries.
  char* load_path_raw = cJSON_GetStringValue(cJSON_GetObjectItem(config, "default_path"));
  char load_path[PATH_MAX];
  resolvePath(load_path, sizeof(load_path), load_path_raw);

  char path[PATH_MAX];
  // Loading Theme
  snprintf(path, sizeof(path), "%s/theme", load_path);

  bool load_result = getThemeFromFile(path, &container->theme_list);
  if (load_result == false) {
    ts_language_delete(container->lang);
    notifyUser(NULL, LOG_WARNING, "Unable to load theme from file '%s'. You can edit path in config file.", path);
    return false;
  }

  // Queries
  snprintf(path, sizeof(path), "%s/queries/highlights-%s.scm", load_path, container->lang_id);

  uint32_t error_offset;
  TSQueryError error_type;
  container->queries = loadQueries(container, path, &error_offset, &error_type);
  if (container->queries == NULL) {
    ts_language_delete(container->lang);
    destroyThemeList(&container->theme_list);
    notifyUser(NULL, LOG_WARNING, "Unable to load queries from file '%s'. You can edit path in config file.", path);
    printQueryLoadError(error_offset, error_type);
    return false;
  }

  // Setup regex map for #match? predicate
  initRegexMap(&container->regex_map);

  // Post processing
  initColorsForTheme(container->theme_list, &color_index, &color_pair);

  container->parser = ts_parser_new();
  ts_parser_set_language(container->parser, container->lang);
  return true;
}


void setFileHighlightDatas(TS_Data* data, LF_LanguageFeature* feature) {
  snprintf(data->lang_id, sizeof(data->lang_id), "%s", feature->id);

  ParserContainer* parser = getParserForLanguage(&parsers, data->lang_id);

  data->is_active = parser != NULL;
  data->tree = NULL;
}


void onStateChangeTS(Action action, TS_Data* data) {

  if (data->is_active == false) {
    return;
  }

  ChangeDescriptor change = actionToChangeDescriptor(action);
  TSInputEdit edit;

  edit.start_byte = change.start_byte;
  edit.new_end_byte = change.new_end_byte;
  edit.old_end_byte = change.old_end_byte;

  edit.start_point.row = change.start_point.row;
  edit.start_point.column = change.start_point.column;

  edit.old_end_point.row = change.old_end_point.row;
  edit.old_end_point.column = change.old_end_point.column;

  edit.new_end_point.row = change.new_end_point.row;
  edit.new_end_point.column = change.new_end_point.column;

  ts_tree_edit(data->tree, &edit);
}

char read_buffer[CHAR_CHUNK_SIZE_TSINPUT * 4];

const char* internalReaderForTree(void* payload, uint32_t byte_index, TSPoint position, uint32_t* bytes_read) {
  PayloadInternalReader* values = payload;
  *bytes_read =
    readNu8CharAtPosition(&values->cursor, position.row, position.column, read_buffer, CHAR_CHUNK_SIZE_TSINPUT);
  return read_buffer;
}

void parseTree(FileNode** root, History** history_frame, TS_Data* highlight_data, History** old_history_frame) {
  if (highlight_data->is_active == false) {
    return;
  }

  ParserContainer* parser = getParserForLanguage(&parsers, highlight_data->lang_id);

  Cursor cursor_root = moduloCursorR(*root, 1, 0);

  PayloadInternalReader reader_payload;
  reader_payload.cursor = cursor_root;

  TSInput input;
  input.encoding = TSInputEncodingUTF8;
  input.read = internalReaderForTree;
  input.payload = &reader_payload;

  TSTree* old_tree = highlight_data->tree;
  highlight_data->tree = ts_parser_parse(parser->parser, highlight_data->tree, input);
  ts_tree_delete(old_tree);

  *old_history_frame = *history_frame;
}


void initRegexMap(RegexMap* regex_map) {
  regex_map->size = 0;
  regex_map->items = NULL;
}

void destroyRegexMap(RegexMap* regex_map) {
  for (int i = 0; i < regex_map->size; i++) {
    regfree(&regex_map->items[i].regex);
  }
  free(regex_map->items);
}

void addRegexPatternToRegexMap(RegexMap* regex_map, const char* pattern, uint32_t regex_id) {
  regex_map->size++;
  regex_map->items = realloc(regex_map->items, sizeof(RegexMapItem) * regex_map->size);
  regex_map->items[regex_map->size - 1].regex_id = regex_id;
  regcomp(&regex_map->items[regex_map->size - 1].regex, pattern, REG_EXTENDED);
}

bool getRegexForRegexId(const RegexMap* regex_map, uint32_t regex_id, regex_t* regex) {
  for (int i = 0; i < regex_map->size; i++) {
    if (regex_map->items[i].regex_id == regex_id) {
      *regex = regex_map->items[i].regex;
      return true;
    }
  }

  return false;
}


char* getNodeContent(TSNode node, Cursor* cursor) {
  uint32_t content_length = ts_node_end_byte(node) - ts_node_start_byte(node);
  if (content_length == 0) {
    return NULL;
  }

  char* content = malloc(content_length + 1);
  if (!content) {
    return NULL;
  }
  fillWithNodeContent(node, cursor, content, content_length + 1);
  return content;
}

int fillWithNodeContent(TSNode node, Cursor* cursor, char* content, int length) {
  uint32_t content_length = ts_node_end_byte(node) - ts_node_start_byte(node);
  int to_read = min(length - 1, content_length);
  TSPoint start_point = ts_node_start_point(node);
  readNBytesAtPosition(cursor, start_point.row, start_point.column, content, to_read);
  content[to_read] = '\0';
  return to_read;
}
