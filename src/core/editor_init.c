#include "editor_init.h"

#include <time.h>

#include "../environnement/global_variables.h"


void initDefaultContext(int file_count, EditorContext* ctx) {
  ctx->file_count = file_count;
  ctx->current_file_index = 0;
  ctx->refresh_local_vars = true;
  ctx->old_history_frame = NULL;
  ctx->mouse_drag = false;
  ctx->peek_c = -1;
  ctx->quit_popup_active = false;
  ctx->force_quit = false;
  initNavigationHistory(&ctx->nav_history);
}

void setupFileExplorer(EditorContext* ctx) {
  char* dir_path = getenv("PWD");
  if (dir_path == NULL) {
    dir_path = getenv("HOME");
  }
  initFolder(workspace_settings.is_used == true ? workspace_settings.dir_path : dir_path, &ctx->pwd);
  ctx->pwd.open = true;
}
