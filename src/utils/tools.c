#include "tools.h"
#include "../advanced/lsp/lsp_client.h"
#include "../data-management/encoding/utf16.h"
#include "../data-management/encoding/utf8.h"
#include "../data-management/file_management.h"

#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "../terminal/key_management.h"

bool areStringEquals(String str1, String str2) { return strcmp(str1.content, str2.content) == 0; }


time_val timeInMilliseconds(void) {
  struct timeval tv;

  gettimeofday(&tv, NULL);
  return (((time_val)tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}

time_val diff2Time(time_val start, time_val end) {
  time_val diff = start - end;
  if (diff < 0) {
    return -diff;
  }
  return diff;
}


int min(int a, int b) {
  if (a < b) {
    return a;
  }
  return b;
}

int max(int a, int b) {
  if (a > b) {
    return a;
  }
  return b;
}

int numberOfDigitOfNumber(int n) {
  char page_number[40];
  sprintf(page_number, "%d", n);
  return strlen(page_number);
}

int powInt(int x, int y) {
  int res = x;

  for (int i = 1; i < y; i++) {
    res *= x;
  }

  return res;
}

/**
 * Give as fileName the absolute path of the file !
 */
unsigned long long hashFileName(char* fileName) {
  int length = strlen(fileName);
  unsigned long long value = powInt(length, 4);

  for (int i = 0; i < length; i++) {
    value += i * i * i * i * fileName[i];
  }

  return value;
}



char* whereis(char* prog) {
  char command[PATH_MAX + 20];
  snprintf(command, sizeof(command), "whereis \"%s\"", prog);

  FILE* f = popen(command, "r");
  if (f == NULL) {
    return NULL;
  }

  char* path = malloc(PATH_MAX);
  if (path == NULL) {
    pclose(f);
    return NULL;
  }
  char tmp_shit[PATH_MAX];
  char fmt[64];
  // Limit fscanf to prevent buffer overflow, respecting PATH_MAX
  snprintf(fmt, sizeof(fmt), " %%%ds %%%ds ", PATH_MAX - 1, PATH_MAX - 1);
  int scan_res = fscanf(f, fmt, tmp_shit, path);
  if (scan_res != 2) {
    free(path);
    pclose(f);
    return NULL;
  }
  pclose(f);

  return path;
}


void encodeURI(const char* src, char* dest, size_t dest_size) {
  const char* p = src;
  size_t written = 0;
  while (*p && (written + 1 < dest_size)) {
    if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~' || *p == '/') {
      *dest++ = *p++;
      written++;
    }
    else {
      if (written + 4 > dest_size) {
        break;
      }
      sprintf(dest, "%%%02X", (unsigned char)*p);
      dest += 3;
      written += 3;
      p++;
    }
  }
  *dest = '\0';
}

void resolvePath(char* dest, size_t dest_size, const char* src) {
  if (src == NULL) {
    dest[0] = '\0';
    return;
  }
  const char* home = getenv("HOME");
  if (home != NULL) {
    if (src[0] == '~') {
      snprintf(dest, dest_size, "%s%s", home, src + 1);
      return;
    }
    // Support legacy %s placeholder
    if (strstr(src, "%s") == src) {
      snprintf(dest, dest_size, "%s%s", home, src + 2);
      return;
    }
  }
  strncpy(dest, src, dest_size - 1);
  dest[dest_size - 1] = '\0';
}

void getLocalURI(char* realive_abs_path, char* uri) {
  char abs_path[PATH_MAX];
  if (realpath(realive_abs_path, abs_path) == NULL) {
    strncpy(abs_path, realive_abs_path, PATH_MAX - 1);
    abs_path[PATH_MAX - 1] = '\0';
  }
  strcpy(uri, "file://");
  encodeURI(abs_path, uri + 7, URI_MAX - 7);
}

bool isDir(char* path) {
  struct stat file_info;
  stat(path, &file_info);
  return S_ISDIR(file_info.st_mode);
}

// copy from http://www.cse.yorku.ca/~oz/hash.html
int hashString(unsigned char* str) {
  unsigned long hash = 5381;
  int c;

  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }

  return (int)hash;
}


char* loadFullFile(const char* path, long* length) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  *length = ftell(f);
  fseek(f, 0, SEEK_SET); /* same as rewind(f); */

  char* string = malloc(*length + 1);
  fread(string, *length, 1, f);
  fclose(f);

  string[*length] = 0;

  return string;
}


// Fonction qui crée récursivement les répertoires comme `mkdir -p`
int mkdir_p(const char* path, mode_t mode) {
  char tmp[1024];
  char* p = NULL;
  size_t len;

  snprintf(tmp, sizeof(tmp), "%s", path);
  len = strlen(tmp);
  if (tmp[len - 1] == '/') {
    tmp[len - 1] = '\0';
  }

  for (p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
        perror("mkdir");
        return -1;
      }
      *p = '/';
    }
  }
  if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
    perror("mkdir");
    return -1;
  }
  return 0;
}


int utf8_get_byte_offset(Char_U8* ch, int element_number, int character_column) {
  int byte_offset = 0;
  for (int i = 0; i < character_column && i < element_number; i++) {
    byte_offset += utf8_size(ch[i]);
  }
  return byte_offset;
}

char* trim(char* ch) {
  while (*ch != '\0' && isblank(*ch)) {
    ch++;
  }
  return ch;
}

static int hex_to_int(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  return -1;
}

void decodeURI(const char* src, char* dest, size_t dest_size) {
  const char* p = src;
  size_t written = 0;

  if (strncmp(p, "file://", 7) == 0) {
    p += 7;
    // Handle 'file:///path' (empty authority) vs 'file://localhost/path' (with authority)
    if (*p != '/') {
      const char* first_slash = strchr(p, '/');
      if (first_slash) {
        p = first_slash;
      }
    }
  }

  while (*p && (written + 1 < dest_size)) {
    if (*p == '%' && isxdigit(p[1]) && isxdigit(p[2])) {
      int high = hex_to_int(p[1]);
      int low = hex_to_int(p[2]);
      if (high != -1 && low != -1) {
        *dest++ = (char)((high << 4) | low);
        p += 3;
        written++;
        continue;
      }
    }
    *dest++ = *p++;
    written++;
  }
  *dest = '\0';
}


CursorDescriptor positionToCursorDescriptor(LSP_Position position) {
  return (CursorDescriptor){.row = LSP_0_row_to_1_row(position.row), .column = position.column};
}


// --- Conversion Helpers ---

LSP_Position LSP_pos_from_cursor(LSP_Server* server_ptr, Cursor cursor) {
  LSP_Server* server = (LSP_Server*)server_ptr;
  int column_offset;
  if (server != NULL && server->position_encoding == LSP_POSITION_ENCODING_UTF32) {
    column_offset = cursor.line_id.absolute_column;
  }
  else if (server != NULL && server->position_encoding == LSP_POSITION_ENCODING_UTF8) {
    int total_line_bytes = cursor.file_id.file->lines_byte_count[cursor.file_id.relative_row - 1];
    int remaining_bytes = byteCountForCurrentLineToEnd(cursor.line_id.line, cursor.line_id.relative_column);
    column_offset = total_line_bytes - remaining_bytes;
  }
  else {
    // Default to UTF-16
    int utf16_offset = 0;
    LineNode* head = getLineForFileIdentifier(cursor.file_id);
    LineNode* current = head;
    while (current != cursor.line_id.line) {
      if (current->byte_count == current->element_number) {
        utf16_offset += current->element_number;
      }
      else {
        for (int i = 0; i < current->element_number; i++) {
          utf16_offset += utf16_length(current->ch[i]);
        }
      }
      current = current->next;
    }
    if (current->byte_count == current->element_number) {
      utf16_offset += cursor.line_id.relative_column;
    }
    else {
      for (int i = 0; i < cursor.line_id.relative_column; i++) {
        utf16_offset += utf16_length(current->ch[i]);
      }
    }
    column_offset = utf16_offset;
  }

  return (LSP_Position){.row = cursor.file_id.absolute_row - 1, .column = column_offset};
}

LSP_Range LSP_range_from_cursor(LSP_Server* server, Cursor c1, Cursor c2) {
  return (LSP_Range){.pos1 = LSP_pos_from_cursor(server, c1), .pos2 = LSP_pos_from_cursor(server, c2)};
}


int LSP_0_row_to_1_row(int lsp_row) {
  if (lsp_row == INT_MAX) {
    return INT_MAX;
  }
  return lsp_row + 1;
}

Cursor LSP_tryToReachCursorForLSPPosition(LSP_Server* server_ptr, Cursor cursor, LSP_Position position) {
  LSP_Server* server = (LSP_Server*)server_ptr;
  if (server != NULL && server->position_encoding == LSP_POSITION_ENCODING_UTF32) {
    return tryToReachAbsPosition(cursor, LSP_0_row_to_1_row(position.row), position.column);
  }
  if (server != NULL && server->position_encoding == LSP_POSITION_ENCODING_UTF8) {
    return byteCursorToCursor(cursor, position.row, position.column);
  }
  // Default to UTF-16
  Cursor target_row = tryToReachAbsPosition(cursor, LSP_0_row_to_1_row(position.row), 0);
  LineNode* current = target_row.line_id.line;
  while (current->prev != NULL) {
    current = current->prev;
  }

  int current_utf16 = 0;
  int char_col = 0;
  while (current != NULL && current_utf16 < position.column) {
    int i = 0;
    for (; i < current->element_number && current_utf16 < position.column; i++) {
      current_utf16 += utf16_length(current->ch[i]);
    }
    char_col += i;
    if (current_utf16 >= position.column) {
      break;
    }
    current = current->next;
  }
  return tryToReachAbsPosition(target_row, LSP_0_row_to_1_row(position.row), char_col);
}
