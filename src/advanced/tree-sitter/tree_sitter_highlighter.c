#include "tree_sitter_highlighter.h"

#include <assert.h>
#include <string.h>
#include <time.h>

#include "../../../lib/tree-sitter/lib/src/tree.h"
#include "../../environnement/constants.h"
#include "../../environnement/global_variables.h"
#include "../../utils/logger.h"
#include "tree_query.h"


void executeHighlightQuery(TSQuery* query, TSQueryCursor* qcursor, RegexMap* regex_map, HighlightThemeList theme_list,
                           Cursor cursor, WindowHighlightDescriptor* highlight_descriptor, int injection_depth);


void saveCaptureToHighlightDescriptor(HighlightThemeList theme_list, Cursor tmp, TSNode node, const char* result,
                                      WindowHighlightDescriptor* highlight_descriptor, uint16_t priority) {
  if (result == NULL) {
    return;
  }

  attr_t attr = A_NORMAL;
  NCURSES_PAIRS_T color = DEFAULT_COLOR_PAIR;

  bool found = false;
  // Setup style for group found.
  for (int i = 0; i < theme_list.size; i++) {
    if (strcmp(theme_list.groups[i].group, result) == 0) {
      attr |= getAttrForTheme(theme_list.groups[i]);
      color = theme_list.groups[i].color_n;

      found = true;
      break;
    }
  }

  if (found == false) {
    // Quit if a query was found, but not theme for this group was found.
    // fprintf(stderr, "      || No theme found for capture %s  || \n", result);
    return;
  }

  TSPoint start_point = ts_node_start_point(node);
  TSPoint end_point = ts_node_end_point(node);

  Cursor begin_cursor = moveRight(byteCursorToCursor(tmp, start_point.row, start_point.column));
  Cursor end_cursor = byteCursorToCursor(tmp, end_point.row, end_point.column);
  whd_insertDescriptor(highlight_descriptor, begin_cursor, end_cursor, color, attr, priority + 1, false,
                       LINE_MARKER_NONE, NULL);
}


void executeInjectionHighlightQuery(Cursor cursor, WindowHighlightDescriptor* highlight_descriptor,
                                    InjectionDescriptor* injection, int injection_depth) {
  // Avoid printing logs for injection parser.
  LogLevel tmp = getNotificationThreshold();
  setNotificationThreshold(LOG_CRITICAL);
  ParserContainer* injected_parser = getParserForLanguage(&parsers, injection->lang_id);
  setNotificationThreshold(tmp);

  if (injected_parser == NULL) {
    return;
  }
  TSRange injected_range;
  injected_range.start_byte = ts_node_start_byte(injection->content_node);
  injected_range.end_byte = ts_node_end_byte(injection->content_node);
  injected_range.start_point = ts_node_start_point(injection->content_node);
  injected_range.end_point = ts_node_end_point(injection->content_node);
  ts_parser_set_included_ranges(injected_parser->parser, &injected_range, 1);

  PayloadInternalReader reader_payload;
  reader_payload.cursor = cursor;

  TSInput input;
  input.encoding = TSInputEncodingUTF8;
  input.read = internalReaderForTree;
  input.payload = &reader_payload;

  TSTree* injected_tree = ts_parser_parse(injected_parser->parser, NULL, input);

  TSQueryCursor* injected_qcursor = ts_query_cursor_new();
  ts_query_cursor_set_point_range(injected_qcursor, injected_range.start_point, injected_range.end_point);

  ts_query_cursor_exec(injected_qcursor, injected_parser->queries, ts_tree_root_node(injected_tree));

  executeHighlightQuery(injected_parser->queries, injected_qcursor, &injected_parser->regex_map,
                        injected_parser->theme_list, cursor, highlight_descriptor, injection_depth + 1);

  ts_query_cursor_delete(injected_qcursor);
  ts_tree_delete(injected_tree);
}

void executeHighlightQuery(TSQuery* query, TSQueryCursor* qcursor, RegexMap* regex_map, HighlightThemeList theme_list,
                           Cursor cursor, WindowHighlightDescriptor* highlight_descriptor, int injection_depth) {
  Cursor tmp = cursor;

  TSQueryMatch query_match;
  InjectionDescriptor injection;
  while (TSQueryCursorNextMatchWithPredicates(&tmp, query, qcursor, &query_match, regex_map, &injection)) {

    // highlight matches
    for (int index = 0; index < query_match.capture_count; index++) {
      TSNode node = query_match.captures[index].node;

      uint32_t size = 0;
      const char* result = ts_query_capture_name_for_id(query, query_match.captures[index].index, &size);

      uint16_t priority = query_match.captures[index].index + injection_depth * 1000;
      // If a capture.
      saveCaptureToHighlightDescriptor(theme_list, tmp, node, result, highlight_descriptor, priority);
    }

    // highlight injection
    if (injection_isActive(&injection)) {
      executeInjectionHighlightQuery(cursor, highlight_descriptor, &injection, injection_depth);
    }
  }
}


void TS_highlightCurrentFile(TS_Data* highlight_data, WINDOW* ftw, int screen_x, int screen_y, Cursor cursor,
                             WindowHighlightDescriptor* highlight_descriptor) {
  if (highlight_data->is_active == false) {
    return;
  }

  clock_t t;
  t = clock();

  // setup parser to iterate on captures
  ParserContainer* parser = getParserForLanguage(&parsers, highlight_data->lang_id);
  assert(parser != NULL);

  int width = getmaxx(ftw);
  int height = getmaxy(ftw);


  // Setup cursor range
  TSQueryCursor* qcursor = ts_query_cursor_new();
  TSPoint begin;
  begin.row = screen_y - 1;
  begin.column = screen_x - 1;
  TSPoint end;
  end.row = screen_y + height - 2;
  end.column = screen_x + width;
  ts_query_cursor_set_point_range(qcursor, begin, end);

  ts_query_cursor_exec(qcursor, parser->queries, ts_tree_root_node(highlight_data->tree));

  // execute the query search.
  executeHighlightQuery(parser->queries, qcursor, &parser->regex_map, parser->theme_list, cursor, highlight_descriptor,
                        0);

  ts_query_cursor_delete(qcursor);
  t = clock() - t;
  double time_taken = ((double)t) / CLOCKS_PER_SEC * 1000; // in millis

  //  fprintf(stderr, "highlight() took %f millis to execute part of check for highlight. \n", time_taken);
}
