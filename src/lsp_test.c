#include <assert.h>
#include <cjson/cJSON.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../lib/tree-sitter/lib/include/tree_sitter/api.h"
#include "advanced/lsp/lsp_client.h"
#include "advanced/lsp/lsp_handler.h"
#include "advanced/tree-sitter/tree_manager.h"
#include "io-management/workspace_settings.h"
#include "utils/tools.h"

//     if (poll(&(struct pollfd){.fd = lsp.inpipefd[0], .events = POLLIN}, 1, 0) == 1) {


int color_pair = 3;
int color_index = 100;
cJSON* config;
ParserList parsers;
LSPServerLinkedList lsp_servers;
WorkspaceSettings loaded_settings;


int main(int argc, char** args) {
  printf("Opening lsp-server.\n");

  LSP_Server lsp;

  // bool result = LSP_openLSPServer("pyright-langserver", "--stdio", &lsp);
  // char* file_name = "assets/main.py";

  // bool result = openLSPServer("pylsp", "-v", &lsp);
  // char* file_name = "assets/main.py";

  bool result = LSP_openLSPServer("clangd", "", "c", &lsp);
  char* file_name = "src/main.c";
  if (result == false) {
    printf("Failed to open lsp-server.\n");
    return 1;
  }
  printf("Server started !\n");

  LSP_initializeServer(&lsp, "al", "1.0", getenv("PWD"));

  // char* init_result_str = cJSON_Print(lsp.init_result);
  // printf("%s\n", init_result_str);
  // free(init_result_str);

  printf("HandShake Success.\n");

  int count = 0;
  while (count < 100) {
    cJSON* request = LSP_readPacketAsJSON(&lsp, false);
    if (request != NULL) {
      printToStdioJSON(request);
      cJSON_Delete(request);
    }

    switch (count) {
      case 10:

        FILE* f = fopen(file_name, "rb");
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET); /* same as rewind(f); */

        char* file_content = malloc(fsize + 1);
        fread(file_content, fsize, 1, f);
        fclose(f);

        file_content[fsize] = 0;


        LSP_notifyLspFileDidOpen(&lsp, file_name, file_content);

        free(file_content);
        break;

      case 50:

        cJSON* tokens_req = cJSON_CreateObject();
        cJSON* text_document = cJSON_AddObjectToObject(tokens_req, "textDocument");
        char uri[URI_MAX];
        getLocalURI(file_name, uri);
        cJSON_AddStringToObject(text_document, "uri", uri);

        LSP_sendPacketWithJSON(&lsp, "textDocument/semanticTokens/full", tokens_req, LSP_REQUEST);

        cJSON_Delete(tokens_req);
        break;
      case 70:

        cJSON* tokens_req_delta = cJSON_CreateObject();
        cJSON_AddStringToObject(tokens_req_delta, "previousResultId", "1");
        cJSON* text_document_delta = cJSON_AddObjectToObject(tokens_req_delta, "textDocument");
        char uri_delta[URI_MAX];
        getLocalURI(file_name, uri_delta);
        cJSON_AddStringToObject(text_document_delta, "uri", uri_delta);

        LSP_sendPacketWithJSON(&lsp, "textDocument/semanticTokens/full/delta", tokens_req_delta, LSP_REQUEST);

        cJSON_Delete(tokens_req_delta);

        break;
      default:
        break;
    }

    usleep(100000);
    count++;
  }

  LSP_closeLSPServer(&lsp);

  return 0;
}
