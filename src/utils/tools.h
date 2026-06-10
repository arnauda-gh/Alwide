#ifndef TOOLS_H
#define TOOLS_H

#include <linux/limits.h>
#include <ncurses.h>
#include <stdint.h>
#include <sys/types.h>

#include "../advanced/lsp/lsp_client.h"
#include "../io-management/io_manager.h"
#include "../terminal/graphics_tools.h"

#define URI_MAX (PATH_MAX * 3 + 8)

typedef struct {
  const char* content;
  uint32_t length;
} String;

bool areStringEquals(String str1, String str2);


typedef long long time_val;

time_val timeInMilliseconds(void);

time_val diff2Time(time_val start, time_val end);

int min(int a, int b);

int max(int a, int b);

int numberOfDigitOfNumber(int n);

unsigned long long hashFileName(char* fileName);


char* whereis(char* prog);

void getLocalURI(char* realive_abs_path, char* uri);

bool isDir(char* path);

int hashString(unsigned char* str);

char* loadFullFile(const char* path, long* length);

int mkdir_p(const char* path, mode_t mode);


int utf8_get_byte_offset(Char_U8* ch, int element_number, int character_column);

char* trim(char* ch);

void decodeURI(const char* src, char* dest, size_t dest_size);

void encodeURI(const char* src, char* dest, size_t dest_size);

void resolvePath(char* dest, size_t dest_size, const char* src);

CursorDescriptor positionToCursorDescriptor(LSP_Position position);

// Strictly 0-based LSP position constructor
// Convert from WishWim (1-based row, 0-based col) to LSP (0-based row, 0-based col)
LSP_Position LSP_pos_from_cursor(LSP_Server* server, Cursor cursor);
LSP_Range LSP_range_from_cursor(LSP_Server* server, Cursor c1, Cursor c2);
Cursor LSP_tryToReachCursorForLSPPosition(LSP_Server* server, Cursor cursor, LSP_Position position);
int LSP_0_row_to_1_row(int lsp_row);


#endif // TOOLS_H
