#ifndef NAVIGATION_HISTORY_H
#define NAVIGATION_HISTORY_H

#include <stdbool.h>

#include "../../utils/tools.h"

#define MAX_NAV_HISTORY 100

typedef struct {
  char file_path[4096];
  int row;
  int column;
  int screen_x;
  int screen_y;
} NavigationLocation;

typedef struct {
  NavigationLocation items[MAX_NAV_HISTORY];
  int size;
} NavigationStack;

typedef struct {
  NavigationStack back_stack;
  NavigationStack forward_stack;

  bool in_typing_session;
  NavigationLocation typing_start_location;
  time_val last_typing_time;
} NavigationHistory;

struct EditorContext;

void initNavigationHistory(NavigationHistory* nh);
bool getActiveNavigationLocation(struct EditorContext* ctx, NavigationLocation* loc);
void pushNavigationPoint(struct EditorContext* ctx);
void navigateBack(struct EditorContext* ctx);
void navigateForward(struct EditorContext* ctx);
void handleNavigationHistoryEvent(struct EditorContext* ctx, const NavigationLocation* prev_loc, int key);

struct WorkspaceSettings;
void restoreNavigationHistory(NavigationHistory* nh, struct WorkspaceSettings* settings);
void saveNavigationHistoryToSettings(struct WorkspaceSettings* settings, NavigationHistory* nh);

#endif // NAVIGATION_HISTORY_H
