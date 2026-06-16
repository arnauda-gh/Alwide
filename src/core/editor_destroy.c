#include "editor_destroy.h"

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../advanced/tree-sitter/tree_manager.h"
#include "../config/language_feature.h"
#include "../data-management/file_management.h"
#include "../environnement/constants.h"
#include "../environnement/global_variables.h"
#include "../io-management/io_explorer.h"
#include "../io-management/workspace_settings.h"
#include "../terminal/highlight.h"
#include "../terminal/kitty_protocol.h"
#include "../terminal/windows/tpw.h"

static void finalizeTerminal() {
  /// --- Teardown Terminal ---
  // Restore terminal state (cursor and mouse) and disable Kitty Keyboard Protocol
  printf("\033[?1003l\033[?1006l\033[0 q\n");
  fflush(stdout);
  kitty_disable();

  // End ncurses and clean input buffer
  usleep(30000);
  flushinp();
  endwin();
}

static void finalizeWorkspace(EditorContext* ctx) {
  /// --- Teardown Workspace ---
  // Free highlight descriptor
  whd_free(&ctx->highlight_descriptor);

  // Save workspace settings if they were used
  if (workspace_settings.is_used == true) {
    WorkspaceSettings new_settings;
    getWorkspaceSettingsForCurrentDir(&new_settings, ctx->files, ctx->file_count, ctx->current_file_index,
                                      ctx->gui_context.ofw_context.ofw_height != 0,
                                      ctx->gui_context.few_context.few_width != 0, FILE_EXPLORER_WIDTH);
    saveNavigationHistoryToSettings(&new_settings, &ctx->nav_history);
    saveWorkspaceSettings(workspace_settings.dir_path, &new_settings);
    destroyWorkspaceSettings(&new_settings);
  }
}

static void finalizeFileData(EditorContext* ctx) {
  /// --- Teardown File Data ---
  // Destroy all file containers
  for (int i = 0; i < ctx->file_count; i++) {
    destroyFileContainer(ctx->files + i);
  }

  // Destroy explorer folder and pwd
  destroyFolder(&ctx->pwd);

  // Free the files array
  free(ctx->files);
}

static void finalizeGlobalSystems() {
  /// --- Teardown Global Systems ---
  // Delete global configuration
  cJSON_Delete(config);

  // Free language features
  destroyLanguageFeatureList(&language_features);

  // Destroy global parser list
  destroyParserList(&parsers);

  // Destroy global LSP server list
  destroyLSPServerList(&lsp_servers);
}

void finalizeEditor(EditorContext* ctx) {
  gui_destroyAllToplevelPopups(&ctx->gui_context);
  finalizeWorkspace(ctx);
  finalizeFileData(ctx);
  finalizeGlobalSystems();
  finalizeTerminal();
}
