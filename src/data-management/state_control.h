#ifndef STATE_CONTROL_H
#define STATE_CONTROL_H

#include "../io-management/io_manager.h"
#include "../utils/tools.h"
#include "file_structure.h"

#define TIME_CONSIDER_UNIQUE_UNDO 300 /*MS*/

#define FILE_STATE_PATH "/tmp/al/state_control/"

void createTmpDir();

typedef enum { INSERT = 'i', DELETE = 'd', DELETE_ONE = 'D', ACTION_NONE = 'n' } ACTION_TYPE;


typedef struct {
  ACTION_TYPE action;
  char unique_ch;
  char* ch;
  time_val time;
  CursorDescriptor cur;
  unsigned int byte_start;
  CursorDescriptor cur_end;
  unsigned int byte_end;
} Action;


typedef struct {
  uint32_t row;
  uint32_t column;
} ChangePoint;

typedef struct {
  uint32_t start_byte;
  uint32_t old_end_byte;
  uint32_t new_end_byte;
  ChangePoint start_point;
  ChangePoint old_end_point;
  ChangePoint new_end_point;
} ChangeDescriptor;


struct History_ {
  uint64_t state_id;
  Action action;

  struct History_* next;
  struct History_* prev;
};

typedef struct History_ History;

#include "../config/language_feature.h"

void initHistory(History* history);

Cursor undo(History** history_p, Cursor cursor, void (*onEachStateChange)(Action action, Cursor* cursor, void* payload),
            void* payload, LF_Tabulation* tab);

Cursor redo(History** history_p, Cursor cursor, void (*onEachStateChange)(Action action, Cursor* cursor, void* payload),
            void* payload, LF_Tabulation* tab);

void saveAction(History** history_p, Action action,
                void (*onEachStateChange)(Action action, Cursor* cursor, void* payload), Cursor* cursor, void* payload);

Cursor doReverseAction(Action* action_p, Cursor cursor,
                       void (*onEachStateChange)(Action action, Cursor* cursor, void* payload), void* payload,
                       LF_Tabulation* tab);

Action createDeleteAction(Cursor cur1, CursorDescriptor cur2);

Action createInsertAction(Cursor cur1, CursorDescriptor cur2);

void destroyAction(Action action);

void destroyEndOfHistory(History* history);

bool saveCurrentStateControl(History root, History* current_state, char* fileName);

void loadCurrentStateControl(History* root, History** current_state, IO_FileID io_file);

void optimizeHistory(History* root, History** history_frame);

ChangeDescriptor actionToChangeDescriptor(Action action);

#endif // STATE_CONTROL_H
