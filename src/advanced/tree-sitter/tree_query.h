#ifndef TREE_QUERY_H
#define TREE_QUERY_H
#include <regex.h>
#include <stdbool.h>

#include <cjson/cJSON.h>
#include <tree_sitter/api.h>
#include "../../environnement/constants.h"
#include "../../utils/tools.h"
#include "tree_manager.h"

typedef struct PredicateStream {
  const TSQueryPredicateStep* predicates;
  uint32_t length;
  uint32_t index;
} PredicateStream;

typedef struct {
  TSQuery* query;
  TSQueryMatch qmatch;
  PredicateStream* stream;
  Cursor* tmp;
  RegexMap* regex_map;
  cJSON* predicate_result;
} ProcessPredicatePayload;

typedef struct {
  TSNode content_node;
  char lang_id[LANG_ID_LENGTH];
} InjectionDescriptor;

///// -------- QUERY --------


void printQueryLoadError(uint32_t error_offset, TSQueryError error_type);

bool TSQueryCursorNextMatchWithPredicates(Cursor* tmp, TSQuery* query, TSQueryCursor* qcursor, TSQueryMatch* qmatch,
                                          RegexMap* regex_map, InjectionDescriptor* injection);

///// -------- QUERY TOOLS --------

String getCaptureString(TSQuery* query, uint32_t index);

bool isStringEqualToNodeContent(Cursor* tmp, String str, TSNode node);

bool isRegexMatchingToNodeContent(Cursor* tmp, regex_t regex, TSNode node);

void injection_init(InjectionDescriptor* self);
bool injection_isActive(InjectionDescriptor* self);
void injection_print(InjectionDescriptor* self, Cursor* cursor);

///// -------- STREAM --------

PredicateStream predicates_stream(const TSQueryPredicateStep* self, uint32_t length);

bool predicates_hasNext(const PredicateStream* self);

TSQueryPredicateStep predicates_peek(const PredicateStream* self);

TSQueryPredicateStep predicates_consume(PredicateStream* self);

TSQueryPredicateStepType predicates_peekType(const PredicateStream* self);

TSQueryPredicateStep predicates_consumeCapture(PredicateStream* self, TSQuery* query, String* str);

TSQueryPredicateStep predicates_consumeString(PredicateStream* self, TSQuery* query, String* str);

void predicates_consumeEND(PredicateStream* self);

#endif // TREE_QUERY_H
