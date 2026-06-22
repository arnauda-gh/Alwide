#ifndef TREE_MANAGER_H
#define TREE_MANAGER_H
#include <regex.h>

#include <tree_sitter/api.h>
#include "../../config/language_feature.h"
#include "../../data-management/state_control.h"
#include "../../environnement/constants.h"
#include "../theme.h"


#define CHAR_CHUNK_SIZE_TSINPUT 500

typedef enum { SYMBOL = 's', FIELD = 'f', REGEX = 'r', GROUP = 'g', MATCH = 'm' } PATH_TYPE;

typedef struct {
  History* history_frame;
  int byte_start;
  int byte_end;
} ActionImprovedWithBytes;

////// ------------------- TOOLS FOR SAVE COMPILED REGEXs -------------------

typedef struct {
  uint32_t regex_id;
  regex_t regex;
} RegexMapItem;

typedef struct {
  int size;
  RegexMapItem* items;
} RegexMap;

void initRegexMap(RegexMap* regex_map);

void destroyRegexMap(RegexMap* regex_map);

void addRegexPatternToRegexMap(RegexMap* regex_map, const char* pattern, uint32_t regex_id);

bool getRegexForRegexId(const RegexMap* regex_map, uint32_t regex_id, regex_t* regex);

////// ------------------- TREE HANDLER -------------------

typedef struct {
  char lang_id[LANG_ID_LENGTH];
  const TSLanguage* lang;
  TSParser* parser;
  TSQuery* queries;
  RegexMap regex_map;
  HighlightThemeList theme_list;
} ParserContainer;

typedef struct {
  ParserContainer* list;
  int size;
} ParserList;

typedef struct {
  char lang_id[LANG_ID_LENGTH];
  bool is_active;
  TSTree* tree;
} TS_Data;


typedef struct {
  Cursor cursor;
} PayloadInternalReader;

const TSLanguage* tree_sitter_c(void);

const TSLanguage* tree_sitter_python(void);

const TSLanguage* tree_sitter_java(void);

const TSLanguage* tree_sitter_cpp(void);

const TSLanguage* tree_sitter_c_sharp(void);

const TSLanguage* tree_sitter_markdown(void);

const TSLanguage* tree_sitter_markdown_inline(void);

const TSLanguage* tree_sitter_make(void);

const TSLanguage* tree_sitter_css(void);

const TSLanguage* tree_sitter_dart(void);

const TSLanguage* tree_sitter_go(void);

const TSLanguage* tree_sitter_javascript(void);

const TSLanguage* tree_sitter_json(void);

const TSLanguage* tree_sitter_bash(void);

const TSLanguage* tree_sitter_query(void);

const TSLanguage* tree_sitter_vhdl(void);

const TSLanguage* tree_sitter_lua(void);

const TSLanguage* tree_sitter_asm(void);

const TSLanguage* tree_sitter_html(void);

const TSLanguage* tree_sitter_latex(void);

const TSLanguage* tree_sitter_ini(void);

void initParserList(ParserList* list);

void destroyParserList(ParserList* list);

void addParserToParserList(ParserList* list, ParserContainer new_parser);

ParserContainer* getParserForLanguage(ParserList* list, char* language);

void getTSLanguageFromString(const TSLanguage** lang, char* language);

bool loadNewParser(ParserContainer* container, char* language);


////// ------------------- QUERIES -------------------


////// ------------------- TREE UTILS -------------------

void setFileHighlightDatas(TS_Data* data, LF_LanguageFeature* feature);

void onStateChangeTS(Action action, TS_Data* data);

const char* internalReaderForTree(void* payload, uint32_t byte_index, TSPoint position, uint32_t* bytes_read);

void parseTree(FileNode** root, History** history_frame, TS_Data* highlight_data, History** old_history_frame);

char* getNodeContent(TSNode node, Cursor* cursor);

int fillWithNodeContent(TSNode node, Cursor* cursor, char* content, int length);

#endif // TREE_MANAGER_H
