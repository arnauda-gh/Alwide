#include "lsp_client.h"

#include <assert.h>
#include <libgen.h>
#include <linux/limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../../utils/tools.h"
#include "../../utils/logger.h"


//////// ---------------- JSON TOOLS ---------------------

void printToStdioJSON(cJSON* json) {
  char* text = cJSON_Print(json);
  printf("%s\n", text);
  free(text);
}


//////// --------------- Low Layer JSON-RPC --------------------------


// https://stackoverflow.com/questions/6171552/popen-simultaneous-read-and-write
bool LSP_openLSPServer(char* name, char* command_args, char* language, LSP_Server* server) {
  char* path = whereis(name);
  if (path == NULL) {
    notifyUser(NULL, LOG_WARNING, "LSP server : '%s' wasn't found ! LSP abort for language %s", name, language);
    return false;
  }
  char pathMemSafe[PATH_MAX];

  strcpy(server->name, name);
  strcpy(pathMemSafe, path);

  free(path);

  snprintf(server->language, 100, "%s", language);

  server->request_id = 0;
  server->response_contexts = NULL;
  server->pending_packets = NULL;
  server->is_init_done = false;
  pthread_mutex_init(&server->initDone, NULL);
  pthread_mutex_lock(&server->initDone);
  pthread_mutex_init(&server->pending_lock, NULL);
  server->on_type_trigger_chars = NULL;
  server->on_type_trigger_chars_count = 0;

  // printf("Starting server on path : %s\n", pathMemSafe);

  // Create pipe in the 2 directions.
  pipe(server->inpipefd);
  pipe(server->outpipefd);

  server->pid = fork();
  if (server->pid == 0) {
    // Child

    // Bind Stdin/Stdout on pipes.
    dup2(server->outpipefd[0], STDIN_FILENO);
    dup2(server->inpipefd[1], STDOUT_FILENO);
    // dup2(server->inpipefd[1], STDERR_FILENO); // Do not bind the err channel it's used for lsp_server logs.


    // ask kernel to deliver SIGTERM in case the parent dies
    prctl(PR_SET_PDEATHSIG, SIGTERM);

    // close unused pipe ends
    close(server->outpipefd[1]);
    close(server->inpipefd[0]);


    char command[strlen(name) + strlen(command_args) + strlen(" 2>> .lsp_logs.txt ") + 1 /*null char*/];
    sprintf(command, "%s %s 2>> .lsp_logs.txt", name, command_args);

    // system(command);
    // execl(pathMemSafe, "", (char *)NULL);

    execl("/bin/sh", "sh", "-c", command, NULL);

    exit(1);
  }
  // The code below will be executed only by parent. You can write and read
  // from the child using pipefd descriptors, and you can send signals to
  // the process using its pid by kill() function. If the child process will
  // exit unexpectedly, the parent process will obtain SIGCHLD signal that
  // can be handled (e.g. you can respawn the child process).

  // close unused pipe ends
  close(server->outpipefd[0]);
  close(server->inpipefd[1]);

  return true;
}


void LSP_closeLSPServer(LSP_Server* server) {
  kill(server->pid, SIGKILL); // send SIGKILL signal to the child process
  int status;
  waitpid(server->pid, &status, 0);
  cJSON_Delete(server->init_result);
  LSP_clearResponseContext(server);

  pthread_mutex_lock(&server->pending_lock);
  LSP_PendingPacket* curr = server->pending_packets;
  while (curr != NULL) {
    LSP_PendingPacket* tmp = curr;
    curr = curr->next;
    free(tmp->method);
    free(tmp->params);
    free(tmp);
  }
  server->pending_packets = NULL;
  pthread_mutex_unlock(&server->pending_lock);

  pthread_mutex_destroy(&server->initDone);
  pthread_mutex_destroy(&server->pending_lock);

  if (server->on_type_trigger_chars != NULL) {
    for (int i = 0; i < server->on_type_trigger_chars_count; i++) {
      free(server->on_type_trigger_chars[i]);
    }
    free(server->on_type_trigger_chars);
    server->on_type_trigger_chars = NULL;
    server->on_type_trigger_chars_count = 0;
  }
}


void skipUntilContent(LSP_Server* server) {}

#define BUFF_SIZE          256
#define HEADER_FIRST_FIELD "Content-Length:"

char* LSP_readPacket(LSP_Server* server) {
  char buf[BUFF_SIZE];

  // read the first head field title.
  int header_size = strlen(HEADER_FIRST_FIELD);
  int n = read(server->inpipefd[0], buf, header_size);

  if (n <= 0) {
    return NULL;
  }
  buf[n] = '\0';

  // If the first header is not the expected one.
  if (n != header_size || strcmp(HEADER_FIRST_FIELD, buf) != 0) {
    fprintf(stderr, "Error with lsp read : expected '%s' but got '%s'.\r\n", HEADER_FIRST_FIELD, buf);
    return NULL;
  }

  // save the value of the first field in buf.
  int index = 0;
  while (index < BUFF_SIZE - 1) {
    if (poll(&(struct pollfd){.fd = server->inpipefd[0], .events = POLLIN}, 1, 100) != 1) {
      break;
    }
    n = read(server->inpipefd[0], buf + index, 1);
    if (n <= 0 || buf[index] == '\n') {
      break;
    }
    index++;
  }
  buf[index] = '\0';

  // Extract the content_length from buf.
  int content_length = 0;
  if (sscanf(buf, " %d ", &content_length) != 1) {
    fprintf(stderr, "Error with lsp read : couldn't extract content length from '%s'.\r\n", buf);
    return NULL;
  }

  // Extract the full second field. (This field is ignored).
  index = 0;
  while (index < BUFF_SIZE - 1) {
    if (poll(&(struct pollfd){.fd = server->inpipefd[0], .events = POLLIN}, 1, 100) != 1) {
      break;
    }
    n = read(server->inpipefd[0], buf + index, 1);
    if (n <= 0 || buf[index] == '\n') {
      break;
    }
    index++;
  }

  // reach first '{' some lsp servers add more break lines...  :/
  index = 0;
  while (index < BUFF_SIZE - 1) {
    if (poll(&(struct pollfd){.fd = server->inpipefd[0], .events = POLLIN}, 1, 100) != 1) {
      break;
    }
    n = read(server->inpipefd[0], buf, 1);
    if (n <= 0 || buf[0] == '{') {
      break;
    }
  }

  // Allocate the content array
  char* content = malloc(content_length * sizeof(char) + 1 /* +1 for null char*/);
  if (content == NULL) {
    return NULL;
  }

  content[0] = '{'; // We already read the first '{'
  n = 0;
  while (n < content_length - 1) {
    if (poll(&(struct pollfd){.fd = server->inpipefd[0], .events = POLLIN}, 1, 1000) != 1) {
      fprintf(stderr, "LSP timeout while reading body.\n");
      break;
    }
    int r = read(server->inpipefd[0], content + 1 + n, content_length - 1 - n);
    if (r <= 0) {
      break;
    }
    n += r;
  }
  content[n + 1] = '\0';

  return content;
}

cJSON* LSP_readPacketAsJSON(LSP_Server* server, bool block) {
  if (block == false && poll(&(struct pollfd){.fd = server->inpipefd[0], .events = POLLIN}, 1, 0) != 1) {
    return NULL;
  }
  char* content_str = LSP_readPacket(server);
  if (content_str == NULL) {
    return NULL;
  }
  cJSON* at_return = cJSON_Parse(content_str);
  if (at_return == NULL) {
    free(content_str);
    return NULL;
  }
  if (LSP_getPacketType(at_return) != LSP_RESPONSE) {
    if (strcmp("window/logMessage", LSP_getPacketMethod(at_return)) == 0) {
      cJSON* message_obj = cJSON_GetObjectItem(LSP_getNotificationParams(at_return), "message");
      fprintf(stderr, "Server log : %s\n", cJSON_GetStringValue(message_obj));
      cJSON_Delete(at_return);
      free(content_str);
      return LSP_readPacketAsJSON(server, block);
    }
  }
  free(content_str);
  return at_return;
}

static int _LSP_sendPacketInternal(LSP_Server* server, char* method, char* params, LSP_PACKET_TYPE type,
                                   LSP_PacketID id) {
  char* content_str;
  cJSON* json_request_obj = NULL;

  if (type == LSP_RESPONSE) {
    // For responses, params already contains the full JSON string
    content_str = strdup(params);
  }
  else {
    if (params == NULL) {
      params = "{}";
    }
    json_request_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(json_request_obj, "jsonrpc", "2.0");
    if (method) {
      cJSON_AddStringToObject(json_request_obj, "method", method);
    }
    cJSON_AddRawToObject(json_request_obj, "params", params);
    if (type == LSP_REQUEST) {
      cJSON_AddNumberToObject(json_request_obj, "id", id);
    }
    content_str = cJSON_PrintUnformatted(json_request_obj);
  }

  int content_length = strlen(content_str);
  char atSend[content_length + 200];

  snprintf(atSend, sizeof(atSend), "Content-Length: %d\r\n\r\n%s", content_length, content_str);
  int head_length = strlen(atSend) - content_length;

  if (write(server->outpipefd[1], atSend, head_length + content_length) < 0) {
    server->pid = -1;
    server->is_init_done = false;
    free(content_str);
    if (json_request_obj) {
      cJSON_Delete(json_request_obj);
    }
    return 0;
  }

  // Logging
  if (type == LSP_RESPONSE) {
    fprintf(stderr, "\n\n ================ RESPONSE ================>>> \n");
  }
  else {
    fprintf(stderr, "\n\n ================ %s ================>>> \n", method ? method : "NOTIFICATION");
  }

  if (params) {
    cJSON* params_2 = cJSON_Parse(params);
    if (params_2) {
      char* text = cJSON_Print(params_2);
      fprintf(stderr, "%s\n", text);
      free(text);
      cJSON_Delete(params_2);
    }
    else {
      fprintf(stderr, "%s\n", params);
    }
  }

  free(content_str);
  if (json_request_obj) {
    cJSON_Delete(json_request_obj);
  }

  if (type == LSP_REQUEST) {
    return id;
  }
  return 0;
}

int LSP_sendPacket(LSP_Server* server, char* method, char* params, LSP_PACKET_TYPE type) {
  // We have to take care about not sending a paquet until the init packet from the lsp has been received.
  // We let the initialize packet be sended to the server.
  if (method != NULL && strcmp(method, "initialize") != 0) {
    if (pthread_mutex_trylock(&server->initDone) != 0) {
      // Buffer packet
      pthread_mutex_lock(&server->pending_lock);
      LSP_PendingPacket* new_packet = malloc(sizeof(LSP_PendingPacket));
      new_packet->method = method ? strdup(method) : NULL;
      new_packet->params = strdup(params ? params : "{}");
      new_packet->type = type;
      new_packet->id = 0;
      if (type == LSP_REQUEST) {
        server->request_id++;
        new_packet->id = server->request_id;
      }
      new_packet->next = NULL;

      if (server->pending_packets == NULL) {
        server->pending_packets = new_packet;
      }
      else {
        LSP_PendingPacket* curr = server->pending_packets;
        while (curr->next != NULL) {
          curr = curr->next;
        }
        curr->next = new_packet;
      }
      int returned_id = new_packet->id;
      pthread_mutex_unlock(&server->pending_lock);
      return returned_id;
    }
    else {
      pthread_mutex_unlock(&server->initDone);
    }
  }

  // Responses don't need to check for initialize but might need buffering if init not done?
  // Actually, if method is NULL and we aren't done with init, we should probably buffer.
  if (method == NULL && pthread_mutex_trylock(&server->initDone) != 0) {
    // Buffer packet (same logic as above)
    pthread_mutex_lock(&server->pending_lock);
    LSP_PendingPacket* new_packet = malloc(sizeof(LSP_PendingPacket));
    new_packet->method = NULL;
    new_packet->params = strdup(params ? params : "{}");
    new_packet->type = type;
    new_packet->id = 0;
    new_packet->next = NULL;

    if (server->pending_packets == NULL) {
      server->pending_packets = new_packet;
    }
    else {
      LSP_PendingPacket* curr = server->pending_packets;
      while (curr->next != NULL) {
        curr = curr->next;
      }
      curr->next = new_packet;
    }
    pthread_mutex_unlock(&server->pending_lock);
    return 0;
  }
  else if (method == NULL) {
    pthread_mutex_unlock(&server->initDone);
  }

  LSP_PacketID packet_id = 0;
  if (type == LSP_REQUEST) {
    server->request_id++;
    packet_id = server->request_id;
  }

  return _LSP_sendPacketInternal(server, method, params, type, packet_id);
}

int LSP_sendPacketWithJSON(LSP_Server* server, char* method, cJSON* content, LSP_PACKET_TYPE type) {
  char* json_params = cJSON_PrintUnformatted(content);
  int temp_id = LSP_sendPacket(server, method, json_params, type);
  free(json_params);
  return temp_id;
}

LSP_PACKET_TYPE LSP_getPacketType(cJSON* content) {
  cJSON* id_obj = cJSON_GetObjectItem(content, "id");
  cJSON* method_obj = cJSON_GetObjectItem(content, "method");
  if (id_obj != NULL && method_obj == NULL) {
    return LSP_RESPONSE;
  }
  if (method_obj != NULL && id_obj == NULL) {
    return LSP_NOTIFICATION;
  }
  if (method_obj != NULL && id_obj != NULL) {
    return LSP_REQUEST;
  }

  // If there something wrong with server happened. Or something not handled.
  assert(false);
  return LSP_RESPONSE;
}


void LSP_addResponseContext(LSP_Server* server, LSP_PacketID id, char* method, char* file_name, void* payload) {
  LSP_ResponseContext* new_context = malloc(sizeof(LSP_ResponseContext));
  new_context->next = server->response_contexts;
  new_context->id = id;
  new_context->method = method;
  snprintf(new_context->file_name, PATH_MAX, "%s", file_name);
  new_context->payload = payload;

  server->response_contexts = new_context;
}


bool LSP_popResponseContext(LSP_Server* server, LSP_PacketID id, LSP_ResponseContext* context) {
  if (server->response_contexts->id == id) {
    *context = *server->response_contexts;
    LSP_ResponseContext* tmp = server->response_contexts;
    server->response_contexts = server->response_contexts->next;
    free(tmp);
    return true;
  }

  LSP_ResponseContext* current = server->response_contexts;
  while (current->next != NULL) {
    if (current->next->id == id) {
      *context = *current->next;
      LSP_ResponseContext* tmp = current->next;
      current->next = tmp->next;
      free(tmp);
      return true;
    }
    current = current->next;
  }

  return false;
}

bool LSP_hasPendingRequest(LSP_Server* server, char* method) {
  LSP_ResponseContext* current = server->response_contexts;
  while (current != NULL) {
    if (strcmp(current->method, method) == 0) {
      return true;
    }
    current = current->next;
  }
  return false;
}

void LSP_clearResponseContext(LSP_Server* server) {
  LSP_ResponseContext* current = server->response_contexts;
  while (current != NULL) {
    LSP_ResponseContext* tmp = current;
    current = current->next;
    free(tmp->payload);
    free(tmp);
  }
}


//////// ----------------- Created Functions ---------------


//// -------- Tools Functions --------

// TODO add capacity when implemented to the init packet
static cJSON* buildInitPacket(LSP_Server* lsp, char* client_name, char* client_version, char* current_workspace_path) {
  cJSON* init_params = cJSON_CreateObject();
  cJSON_AddNumberToObject(init_params, "processId", lsp->pid);

  char workspaceURI[URI_MAX];
  getLocalURI(current_workspace_path, workspaceURI);

  cJSON_AddStringToObject(init_params, "rootUri", workspaceURI);
  cJSON_AddStringToObject(init_params, "rootPath", current_workspace_path);

  cJSON* client_info = cJSON_AddObjectToObject(init_params, "clientInfo");
  cJSON_AddStringToObject(client_info, "name", client_name);
  cJSON_AddStringToObject(client_info, "version", client_version);

  cJSON* capabilities = cJSON_AddObjectToObject(init_params, "capabilities");

  cJSON* general = cJSON_AddObjectToObject(capabilities, "general");
  cJSON* positionEncodings = cJSON_AddArrayToObject(general, "positionEncodings");
  cJSON_AddItemToArray(positionEncodings, cJSON_CreateString("utf-32"));
  cJSON_AddItemToArray(positionEncodings, cJSON_CreateString("utf-8"));
  cJSON_AddItemToArray(positionEncodings, cJSON_CreateString("utf-16"));

  cJSON* textDocument = cJSON_AddObjectToObject(capabilities, "textDocument");

  // Synchronization
  cJSON* sync = cJSON_AddObjectToObject(textDocument, "synchronization");
  cJSON_AddBoolToObject(sync, "dynamicRegistration", true);
  cJSON_AddBoolToObject(sync, "willSave", true);
  cJSON_AddBoolToObject(sync, "didSave", true);

  // Completion
  cJSON* completion = cJSON_AddObjectToObject(textDocument, "completion");
  cJSON_AddBoolToObject(completion, "dynamicRegistration", true);
  cJSON* completionItem = cJSON_AddObjectToObject(completion, "completionItem");
  cJSON_AddBoolToObject(completionItem, "snippetSupport", false);

  // Formatting
  cJSON* formatting = cJSON_AddObjectToObject(textDocument, "formatting");
  cJSON_AddBoolToObject(formatting, "dynamicRegistration", true);

  // On-Type Formatting
  cJSON* onType = cJSON_AddObjectToObject(textDocument, "onTypeFormatting");
  cJSON_AddBoolToObject(onType, "dynamicRegistration", true);

  cJSON* signatureHelp = cJSON_AddObjectToObject(textDocument, "signatureHelp");
  cJSON_AddBoolToObject(signatureHelp, "dynamicRegistration", true);
  cJSON* signatureInformation = cJSON_AddObjectToObject(signatureHelp, "signatureInformation");
  cJSON* parameterInformation = cJSON_AddObjectToObject(signatureInformation, "parameterInformation");
  cJSON_AddBoolToObject(parameterInformation, "labelOffsetSupport", true);

  cJSON* codeAction = cJSON_AddObjectToObject(textDocument, "codeAction");
  cJSON_AddBoolToObject(codeAction, "dynamicRegistration", true);
  cJSON* codeActionLiteralSupport = cJSON_AddObjectToObject(codeAction, "codeActionLiteralSupport");
  cJSON* codeActionKind = cJSON_AddObjectToObject(codeActionLiteralSupport, "codeActionKind");
  cJSON* valueSet = cJSON_AddArrayToObject(codeActionKind, "valueSet");
  cJSON_AddItemToArray(valueSet, cJSON_CreateString("quickfix"));
  cJSON_AddItemToArray(valueSet, cJSON_CreateString("refactor"));
  cJSON_AddBoolToObject(codeAction, "isPreferredSupport", true);


  cJSON* workspace_cap = cJSON_AddObjectToObject(capabilities, "workspace");
  cJSON_AddBoolToObject(workspace_cap, "applyEdit", true);
  cJSON* executeCommand = cJSON_AddObjectToObject(workspace_cap, "executeCommand");
  cJSON_AddBoolToObject(executeCommand, "dynamicRegistration", true);

  cJSON* workspace_array = cJSON_AddArrayToObject(init_params, "workspaceFolders");

  cJSON* workspace = cJSON_CreateObject();
  cJSON_AddStringToObject(workspace, "uri", workspaceURI);
  cJSON_AddStringToObject(workspace, "name", basename(current_workspace_path));
  cJSON_AddItemToArray(workspace_array, workspace);

  return init_params;
}

static void extractDataFromServerCapability(LSP_Server* lsp) {
  // Extract on-type trigger characters
  cJSON* server_capabilities = cJSON_GetObjectItem(lsp->init_result, "capabilities");

  // Default to UTF-16
  lsp->position_encoding = LSP_POSITION_ENCODING_UTF16;

  if (server_capabilities) {
    cJSON* positionEncoding = cJSON_GetObjectItem(server_capabilities, "positionEncoding");
    if (positionEncoding && cJSON_IsString(positionEncoding)) {
      char* encoding_str = cJSON_GetStringValue(positionEncoding);
      if (strcmp(encoding_str, "utf-32") == 0) {
        lsp->position_encoding = LSP_POSITION_ENCODING_UTF32;
      }
      else if (strcmp(encoding_str, "utf-8") == 0) {
        lsp->position_encoding = LSP_POSITION_ENCODING_UTF8;
      }
    }

    cJSON* onType = cJSON_GetObjectItem(server_capabilities, "documentOnTypeFormattingProvider");
    if (onType) {
      cJSON* first = cJSON_GetObjectItem(onType, "firstTriggerCharacter");
      cJSON* more = cJSON_GetObjectItem(onType, "moreTriggerCharacter");
      int count = (first ? 1 : 0) + (more ? cJSON_GetArraySize(more) : 0);
      if (count > 0) {
        lsp->on_type_trigger_chars = malloc(sizeof(char*) * count);
        lsp->on_type_trigger_chars_count = 0;
        if (first && cJSON_IsString(first)) {
          lsp->on_type_trigger_chars[lsp->on_type_trigger_chars_count++] = strdup(first->valuestring);
        }
        if (more && cJSON_IsArray(more)) {
          cJSON* item;
          cJSON_ArrayForEach(item, more) {
            if (cJSON_IsString(item)) {
              lsp->on_type_trigger_chars[lsp->on_type_trigger_chars_count++] = strdup(item->valuestring);
            }
          }
        }
      }
    }
  }
}

// TODO Implement send of Capabality
// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#initialize
void LSP_initializeServer(LSP_Server* lsp, char* client_name, char* client_version, char* current_workspace_path) {
  cJSON* init_params = buildInitPacket(lsp, client_name, client_version, current_workspace_path);
  int tmp_id = LSP_sendPacketWithJSON(lsp, "initialize", init_params, LSP_REQUEST);

  // print for logs. to delete.
  char* init_params_text = cJSON_Print(init_params);
  fprintf(stderr, "INIT_SEND:\n%s\n", init_params_text);
  free(init_params_text);

  cJSON_Delete(init_params);

  // Wait for the lsp init packet to be received
  cJSON* content = LSP_readPacketAsJSON(lsp, true);
  // this assert will also check if the packet is a request.
  assert(tmp_id == LSP_getPacketID(content));
  lsp->init_result = LSP_extractPacketResult(content);

  // Extract data from the packet just received from the lsp server.
  extractDataFromServerCapability(lsp);

  // print for logs. to delete.
  char* init_text = cJSON_Print(lsp->init_result);
  fprintf(stderr, "INIT: \n%s\n", init_text);
  free(init_text);

  cJSON_Delete(content);
  lsp->is_init_done = true;
  pthread_mutex_unlock(&lsp->initDone);

  // Mandatory 'initialized' notification after initialization handshake
  LSP_sendPacket(lsp, "initialized", "{}", LSP_NOTIFICATION);

  // Flush pending packet to send
  pthread_mutex_lock(&lsp->pending_lock);
  LSP_PendingPacket* curr = lsp->pending_packets;
  while (curr != NULL) {
    _LSP_sendPacketInternal(lsp, curr->method, curr->params, curr->type, curr->id);
    LSP_PendingPacket* tmp = curr;
    curr = curr->next;
    free(tmp->method);
    free(tmp->params);
    free(tmp);
  }
  lsp->pending_packets = NULL;
  pthread_mutex_unlock(&lsp->pending_lock);
}


cJSON* LSP_extractPacketResult(cJSON* response_obj) {
  cJSON* at_return = cJSON_GetObjectItem(response_obj, "result");
  cJSON_DetachItemFromObject(response_obj, "result");
  return at_return;
}

LSP_PacketID LSP_getPacketID(cJSON* request_body) {
  assert(LSP_getPacketType(request_body) == LSP_REQUEST || LSP_getPacketType(request_body) == LSP_RESPONSE);
  cJSON* id_obj = cJSON_GetObjectItem(request_body, "id");
  assert(id_obj != NULL);
  unsigned long current_id = (unsigned long)cJSON_GetNumberValue(id_obj);
  return current_id;
}

char* LSP_getPacketMethod(cJSON* request_body) {
  assert(LSP_getPacketType(request_body) == LSP_REQUEST || LSP_getPacketType(request_body) == LSP_NOTIFICATION);
  cJSON* method_obj = cJSON_GetObjectItem(request_body, "method");
  assert(method_obj != NULL);
  char* method_name = cJSON_GetStringValue(method_obj);
  return method_name;
}

cJSON* LSP_getPacketResult(cJSON* request_body) {
  assert(LSP_getPacketType(request_body) == LSP_REQUEST || LSP_getPacketType(request_body) == LSP_RESPONSE);
  cJSON* result = cJSON_GetObjectItem(request_body, "result");
  assert(result != NULL);
  return result;
}


cJSON* LSP_getNotificationParams(cJSON* notification_body) {
  assert(LSP_getPacketType(notification_body) == LSP_NOTIFICATION);
  cJSON* param_obj = cJSON_GetObjectItem(notification_body, "params");
  return param_obj;
}

LSP_Position LSP_pos(int lsp_row, int lsp_col) { return (LSP_Position){.row = lsp_row, .column = lsp_col}; }

LSP_Position LSP_getPositionFromJSON(cJSON* json) {
  return LSP_pos(cJSON_GetNumberValue(cJSON_GetObjectItem(json, "line")),
                 cJSON_GetNumberValue(cJSON_GetObjectItem(json, "character")));
}

cJSON* LSP_getJSONPosition(LSP_Position pos) {
  cJSON* position = cJSON_CreateObject();
  cJSON_AddNumberToObject(position, "line", pos.row);
  cJSON_AddNumberToObject(position, "character", pos.column);

  return position;
}


LSP_Range LSP_range(LSP_Position p1, LSP_Position p2) { return (LSP_Range){.pos1 = p1, .pos2 = p2}; }

LSP_Range LSP_getRangeFromJSON(cJSON* json) {
  return LSP_range(LSP_getPositionFromJSON(cJSON_GetObjectItem(json, "start")),
                   LSP_getPositionFromJSON(cJSON_GetObjectItem(json, "end")));
}

cJSON* LSP_getJSONRange(LSP_Range range) {
  cJSON* range_json = cJSON_CreateObject();
  cJSON_AddItemToObject(range_json, "start", LSP_getJSONPosition(range.pos1));
  cJSON_AddItemToObject(range_json, "end", LSP_getJSONPosition(range.pos2));

  return range_json;
}

LSP_TextDocumentItem LSP_getTextDocumentItemOf(char* file_name, char* languageId, int version, char* text) {
  LSP_TextDocumentItem text_document_item;
  strncpy(text_document_item.file_name, file_name, PATH_MAX);
  strncpy(text_document_item.languageId, languageId, LANGUAGE_ID_LENGTH);
  text_document_item.version = version;
  text_document_item.text = strdup(text);

  return text_document_item;
}

cJSON* LSP_getJSONTextDocumentItem(char* file_name, char* languageId, int version, char* text) {
  cJSON* text_document = cJSON_CreateObject();

  char uri[URI_MAX];
  getLocalURI(file_name, uri);
  cJSON_AddStringToObject(text_document, "uri", uri);
  cJSON_AddStringToObject(text_document, "languageId", languageId);
  cJSON_AddNumberToObject(text_document, "version", version);

  cJSON_AddStringToObject(text_document, "text", text);

  return text_document;
}

LSP_TextDocumentItem LSP_getTextDocumentItemFromJSON(cJSON* json) {
  return LSP_getTextDocumentItemOf(cJSON_GetStringValue(cJSON_GetObjectItem(json, "uri")),
                                   cJSON_GetStringValue(cJSON_GetObjectItem(json, "languageId")),
                                   cJSON_GetNumberValue(cJSON_GetObjectItem(json, "version")),
                                   cJSON_GetStringValue(cJSON_GetObjectItem(json, "text")));
}

void LSP_destroyTextDocumentItem(LSP_TextDocumentItem text_document_item) { free(text_document_item.text); }

LSP_TextDocumentIdentifier LSP_getTextDocumentIdentifierOf(char* file_name) {
  LSP_TextDocumentIdentifier text_id;
  strncpy(text_id.file_name, file_name, PATH_MAX);

  return text_id;
}

cJSON* LSP_getJSONTextDocumentIdentifier(char* file_name) {
  cJSON* text_document_id = cJSON_CreateObject();
  char uri[URI_MAX];
  getLocalURI(file_name, uri);
  cJSON_AddStringToObject(text_document_id, "uri", uri);

  return text_document_id;
}

cJSON* LSP_getJSONTextDocumentIdentifierVersionned(char* file_name, int version) {
  cJSON* text_document_id = cJSON_CreateObject();
  char uri[URI_MAX];
  getLocalURI(file_name, uri);
  cJSON_AddStringToObject(text_document_id, "uri", uri);
  cJSON_AddNumberToObject(text_document_id, "version", version);

  return text_document_id;
}

LSP_TextDocumentIdentifier LSP_getTextDocumentIdentifierFromJSON(cJSON* json) {
  char path[PATH_MAX];
  decodeURI(cJSON_GetStringValue(cJSON_GetObjectItem(json, "uri")), path, PATH_MAX);
  return LSP_getTextDocumentIdentifierOf(path);
}

void LSP_destroyTextDocumentIdentifier(LSP_TextDocumentIdentifier* text_document_identifier) {}

LSP_TextDocumentPositionParams LSP_getTextDocumentPositionParamsOf(char* file_name, LSP_Position pos) {
  LSP_TextDocumentPositionParams text_document_position_params;
  text_document_position_params.text_id = LSP_getTextDocumentIdentifierOf(file_name);
  text_document_position_params.position = pos;

  return text_document_position_params;
}

cJSON* LSP_getJSONTextDocumentPositionParams(char* file_name, LSP_Position pos) {
  cJSON* text_document_position_params = cJSON_CreateObject();

  cJSON* text_document_id = LSP_getJSONTextDocumentIdentifier(file_name);
  cJSON_AddItemToObject(text_document_position_params, "textDocument", text_document_id);

  cJSON* position = LSP_getJSONPosition(pos);
  cJSON_AddItemToObject(text_document_position_params, "position", position);

  return text_document_position_params;
}

LSP_TextDocumentPositionParams LSP_getTextDocumentPositionParamsFromJSON(cJSON* json) {
  LSP_TextDocumentIdentifier text_id = LSP_getTextDocumentIdentifierFromJSON(cJSON_GetObjectItem(json, "textDocument"));
  LSP_Position position = LSP_getPositionFromJSON(cJSON_GetObjectItem(json, "position"));
  return LSP_getTextDocumentPositionParamsOf(text_id.file_name, position);
}

void LSP_destroyTextDocumentPositionParams(LSP_TextDocumentPositionParams text_document_position_params) {
  LSP_destroyTextDocumentIdentifier(&text_document_position_params.text_id);
}


LSP_TextEdit LSP_getTextEditOf(LSP_Range range, char* new_text) {
  LSP_TextEdit text_edit;
  text_edit.range = range;
  text_edit.new_text = strdup(new_text);
  return text_edit;
}


cJSON* LSP_getJSONTextEdit(LSP_Range range, char* new_text) {
  cJSON* text_edit = cJSON_CreateObject();

  cJSON* range_json = LSP_getJSONRange(range);
  cJSON_AddItemToObject(text_edit, "range", range_json);

  cJSON_AddStringToObject(text_edit, "newText", new_text);

  return text_edit;
}

LSP_TextEdit LSP_getTextEditFromJSON(cJSON* json) {
  LSP_Range range = LSP_getRangeFromJSON(cJSON_GetObjectItem(json, "range"));
  return LSP_getTextEditOf(range, cJSON_GetStringValue(cJSON_GetObjectItem(json, "newText")));
}

void LSP_destroyTextEdit(LSP_TextEdit text_edit) { free(text_edit.new_text); }

LSP_TextDocumentEdit LSP_getTextDocumentEditFromJSON(cJSON* json) {
  LSP_TextDocumentEdit text_document_edit;
  cJSON* text_document = cJSON_GetObjectItem(json, "textDocument");
  cJSON* edits = cJSON_GetObjectItem(json, "edits");

  text_document_edit.text_document = LSP_getTextDocumentIdentifierFromJSON(text_document);
  text_document_edit.edits_count = cJSON_GetArraySize(edits);
  text_document_edit.edits = malloc(sizeof(LSP_TextEdit) * text_document_edit.edits_count);

  for (int i = 0; i < text_document_edit.edits_count; i++) {
    text_document_edit.edits[i] = LSP_getTextEditFromJSON(cJSON_GetArrayItem(edits, i));
  }

  return text_document_edit;
}

void LSP_destroyTextDocumentEdit(LSP_TextDocumentEdit* text_document_edit) {
  LSP_destroyTextDocumentIdentifier(&text_document_edit->text_document);
  for (int i = 0; i < text_document_edit->edits_count; i++) {
    LSP_destroyTextEdit(text_document_edit->edits[i]);
  }
  free(text_document_edit->edits);
}

LSP_WorkspaceEdit LSP_getWorkspaceEditFromJSON(cJSON* json) {
  LSP_WorkspaceEdit workspace_edit = {0};
  if (!json || cJSON_IsNull(json)) {
    return workspace_edit;
  }

  // LSP 3.17 uses 'documentChanges' or 'changes'
  cJSON* document_changes = cJSON_GetObjectItem(json, "documentChanges");
  if (cJSON_IsArray(document_changes)) {
    workspace_edit.document_changes_count = cJSON_GetArraySize(document_changes);
    workspace_edit.document_changes = malloc(sizeof(LSP_TextDocumentEdit) * workspace_edit.document_changes_count);
    for (int i = 0; i < workspace_edit.document_changes_count; i++) {
      workspace_edit.document_changes[i] = LSP_getTextDocumentEditFromJSON(cJSON_GetArrayItem(document_changes, i));
    }
  }
  else {
    cJSON* changes = cJSON_GetObjectItem(json, "changes");
    if (cJSON_IsObject(changes)) {
      workspace_edit.document_changes_count = cJSON_GetArraySize(changes);
      workspace_edit.document_changes = malloc(sizeof(LSP_TextDocumentEdit) * workspace_edit.document_changes_count);
      int i = 0;
      cJSON* file_edits = NULL;
      cJSON_ArrayForEach(file_edits, changes) {
        char path[PATH_MAX];
        decodeURI(file_edits->string, path, PATH_MAX);
        workspace_edit.document_changes[i].text_document = LSP_getTextDocumentIdentifierOf(path);
        workspace_edit.document_changes[i].edits_count = cJSON_GetArraySize(file_edits);
        workspace_edit.document_changes[i].edits =
          malloc(sizeof(LSP_TextEdit) * workspace_edit.document_changes[i].edits_count);
        for (int j = 0; j < workspace_edit.document_changes[i].edits_count; j++) {
          workspace_edit.document_changes[i].edits[j] = LSP_getTextEditFromJSON(cJSON_GetArrayItem(file_edits, j));
        }
        i++;
      }
    }
  }
  return workspace_edit;
}

void LSP_destroyWorkspaceEdit(LSP_WorkspaceEdit* workspace_edit) {
  if (!workspace_edit) {
    return;
  }
  for (int i = 0; i < workspace_edit->document_changes_count; i++) {
    LSP_destroyTextDocumentEdit(&workspace_edit->document_changes[i]);
  }
  free(workspace_edit->document_changes);
  workspace_edit->document_changes = NULL;
  workspace_edit->document_changes_count = 0;
}

LSP_CodeAction LSP_getCodeActionFromJSON(cJSON* json) {
  LSP_CodeAction code_action = {0};
  cJSON* title_item = cJSON_GetObjectItem(json, "title");
  cJSON* kind_item = cJSON_GetObjectItem(json, "kind");
  cJSON* isPreferred_item = cJSON_GetObjectItem(json, "isPreferred");
  cJSON* edit_item = cJSON_GetObjectItem(json, "edit");
  cJSON* command_item = cJSON_GetObjectItem(json, "command");

  if (cJSON_IsString(title_item)) {
    snprintf(code_action.title, 200, "%s", title_item->valuestring);
  }
  if (cJSON_IsString(kind_item)) {
    snprintf(code_action.kind, 50, "%s", kind_item->valuestring);
  }
  code_action.isPreferred = cJSON_IsTrue(isPreferred_item);

  if (edit_item && !cJSON_IsNull(edit_item)) {
    code_action.edit = LSP_getWorkspaceEditFromJSON(edit_item);
    code_action.has_edit = true;
  }

  if (command_item && !cJSON_IsNull(command_item)) {
    cJSON* cmd_str = cJSON_GetObjectItem(command_item, "command");
    cJSON* cmd_args = cJSON_GetObjectItem(command_item, "arguments");
    if (cmd_str && cJSON_IsString(cmd_str)) {
      snprintf(code_action.command.command, 100, "%s", cmd_str->valuestring);
      if (cmd_args) {
        code_action.command.arguments = cJSON_Duplicate(cmd_args, true);
      }
      code_action.has_command = true;
      // If title is missing at root, try to get it from command
      if (code_action.title[0] == '\0') {
        cJSON* cmd_title = cJSON_GetObjectItem(command_item, "title");
        if (cJSON_IsString(cmd_title)) {
          snprintf(code_action.title, 200, "%s", cmd_title->valuestring);
        }
      }
    }
  }

  return code_action;
}

void LSP_destroyCodeAction(LSP_CodeAction* code_action) {
  if (!code_action) {
    return;
  }
  LSP_destroyWorkspaceEdit(&code_action->edit);
  if (code_action->command.arguments) {
    cJSON_Delete(code_action->command.arguments);
    code_action->command.arguments = NULL;
  }
}

void LSP_initDiagnosticList(LSP_DiagnosticList* list) {
  list->items = NULL;
  list->size = 0;
}

void LSP_destroyDiagnosticList(LSP_DiagnosticList* list) {
  if (!list) {
    return;
  }
  if (list->items) {
    for (int i = 0; i < list->size; i++) {
      LSP_destroyDiagnostic(list->items + i);
    }
    free(list->items);
    list->items = NULL;
  }
  list->size = 0;
}

void LSP_initCodeActionList(LSP_CodeActionList* list) {
  list->items = NULL;
  list->size = 0;
}

void LSP_destroyCodeActionList(LSP_CodeActionList* list) {
  if (!list) {
    return;
  }
  if (list->items) {
    for (int i = 0; i < list->size; i++) {
      LSP_destroyCodeAction(list->items + i);
    }
    free(list->items);
    list->items = NULL;
  }
  list->size = 0;
}

void LSP_sendResponse(LSP_Server* server, LSP_PacketID id, cJSON* result) {
  cJSON* response = cJSON_CreateObject();
  cJSON_AddStringToObject(response, "jsonrpc", "2.0");
  cJSON_AddNumberToObject(response, "id", id);
  if (result) {
    cJSON_AddItemToObject(response, "result", result);
  }
  else {
    cJSON_AddNullToObject(response, "result");
  }

  char* text = cJSON_PrintUnformatted(response);
  LSP_sendPacket(server, NULL, text, LSP_RESPONSE);
  free(text);
  cJSON_Delete(response);
}

void LSP_requestExecuteCommand(LSP_Server* lsp, char* file_name, LSP_Command* command) {
  cJSON* request_content = cJSON_CreateObject();
  cJSON_AddStringToObject(request_content, "command", command->command);
  if (command->arguments) {
    cJSON_AddItemToObject(request_content, "arguments", cJSON_Duplicate(command->arguments, true));
  }

  LSP_PacketID id = LSP_sendPacketWithJSON(lsp, "workspace/executeCommand", request_content, LSP_REQUEST);
  LSP_addResponseContext(lsp, id, "workspace/executeCommand", file_name, NULL);

  cJSON_Delete(request_content);
}

LSP_Location LSP_getLocationOf(char* file_name, LSP_Range range) {
  LSP_Location location;
  location.file_name = LSP_getTextDocumentIdentifierOf(file_name);
  location.range = range;
  return location;
}

cJSON* LSP_getJSONLocation(char* file_name, LSP_Range range) {
  cJSON* location = cJSON_CreateObject();

  char uri[URI_MAX];
  getLocalURI(file_name, uri);
  cJSON_AddStringToObject(location, "uri", uri);

  cJSON* range_json = LSP_getJSONRange(range);
  cJSON_AddItemToObject(location, "range", range_json);

  return location;
}

LSP_Location LSP_getLocationFromJSON(cJSON* json) {
  LSP_TextDocumentIdentifier text_id = LSP_getTextDocumentIdentifierFromJSON(cJSON_GetObjectItem(json, "uri"));
  LSP_Range range = LSP_getRangeFromJSON(cJSON_GetObjectItem(json, "range"));

  return LSP_getLocationOf(text_id.file_name, range);
}

void LSP_destroyLocation(LSP_Location* location) { LSP_destroyTextDocumentIdentifier(&location->file_name); }


// Internal helper for handling Location and LocationLink
LSP_Location LSP_getLocationOrLocationLinkFromJSON_internal(cJSON* json) {
  cJSON* uri_obj = cJSON_GetObjectItem(json, "uri");
  if (uri_obj == NULL) {
    uri_obj = cJSON_GetObjectItem(json, "targetUri");
  }

  // we abstract the range system to just save the range where to jump. Prioritizing precises ranges.
  cJSON* range_obj = cJSON_GetObjectItem(json, "range");
  if (range_obj == NULL) {
    range_obj = cJSON_GetObjectItem(json, "targetSelectionRange");
  }
  if (range_obj == NULL) {
    range_obj = cJSON_GetObjectItem(json, "targetRange");
  }

  char* uri = cJSON_GetStringValue(uri_obj);
  char decoded_path[PATH_MAX];
  decodeURI(uri, decoded_path, PATH_MAX);

  LSP_TextDocumentIdentifier text_id;
  strncpy(text_id.file_name, decoded_path, PATH_MAX);
  LSP_Range range = LSP_getRangeFromJSON(range_obj);

  return (LSP_Location){.file_name = text_id, .range = range};
}

void LSP_getLocationArrayFromJSON(cJSON* json, LSP_LocationArray* array) {
  if (json == NULL || cJSON_IsNull(json)) {
    array->size = 0;
    array->items = NULL;
    return;
  }

  if (cJSON_IsArray(json)) {
    int count = cJSON_GetArraySize(json);
    array->size = count;
    array->items = malloc(sizeof(LSP_Location) * count);
    for (int i = 0; i < count; i++) {
      cJSON* item = cJSON_GetArrayItem(json, i);
      // Handle Location or LocationLink
      cJSON* uri = cJSON_GetObjectItem(item, "uri");
      if (uri == NULL) {
        // LocationLink has targetUri instead
        uri = cJSON_GetObjectItem(item, "targetUri");
      }
      cJSON* range = cJSON_GetObjectItem(item, "range");
      if (range == NULL) {
        // LocationLink has targetRange or targetSelectionRange
        range = cJSON_GetObjectItem(item, "targetRange");
      }

      array->items[i] = LSP_getLocationOrLocationLinkFromJSON_internal(item);
    }
  }
  else {
    array->size = 1;
    array->items = malloc(sizeof(LSP_Location));
    array->items[0] = LSP_getLocationOrLocationLinkFromJSON_internal(json);
  }
}


void LSP_destroyLocationArray(LSP_LocationArray* array) {
  if (array->items) {
    for (int i = 0; i < array->size; i++) {
      LSP_destroyLocation(array->items + i);
    }
    free(array->items);
    array->items = NULL;
  }
  array->size = 0;
}


void initDiagnostic(LSP_Diagnostic* diagnostic) {
  diagnostic->code[0] = '\0';
  diagnostic->message[0] = '\0';
  diagnostic->codeDescription[0] = '\0';
  diagnostic->tags[0] = LSP_DIAG_TAG_NONE;
  diagnostic->severity = LSP_DIAG_SEVERITY_NONE;
  diagnostic->source[0] = '\0';
}

LSP_Diagnostic LSP_getDiagnosticOf(LSP_Range range) {
  LSP_Diagnostic diagnostic;
  initDiagnostic(&diagnostic);
  diagnostic.range = range;
  return diagnostic;
}

cJSON* LSP_getJSONDiagnostic(LSP_Range range) {
  cJSON* diagnostic = cJSON_CreateObject();
  cJSON_AddItemToObject(diagnostic, "range", LSP_getJSONRange(range));
  return diagnostic;
}

LSP_Diagnostic LSP_getDiagnosticFromJSON(cJSON* json) {
  LSP_Diagnostic diagnostic;
  initDiagnostic(&diagnostic);

  diagnostic.range = LSP_getRangeFromJSON(cJSON_GetObjectItem(json, "range"));

  cJSON* code = cJSON_GetObjectItem(json, "code");
  if (code) {
    if (cJSON_GetStringValue(code)) {
      snprintf(diagnostic.code, MESSAGE_LENGTH, "%s", cJSON_GetStringValue(code));
    }
  }

  cJSON* severity = cJSON_GetObjectItem(json, "severity");
  if (severity) {
    diagnostic.severity = (int)cJSON_GetNumberValue(severity);
  }

  cJSON* message = cJSON_GetObjectItem(json, "message");
  if (message) {
    if (cJSON_GetStringValue(message)) {
      snprintf(diagnostic.message, MESSAGE_LENGTH, "%s", cJSON_GetStringValue(message));
    }
  }

  cJSON* codeDescription = cJSON_GetObjectItem(json, "codeDescription");
  if (codeDescription) {
    if (cJSON_GetStringValue(codeDescription)) {
      snprintf(diagnostic.codeDescription, MESSAGE_LENGTH, "%s", cJSON_GetStringValue(codeDescription));
    }
  }

  return diagnostic;
}

void LSP_destroyDiagnostic(LSP_Diagnostic* diagnostic) {}


void LSP_getCompletionListFromJSON(cJSON* json, LSP_CompletionList* list) {
  assert(json != NULL);

  // isIncomplete
  cJSON* isIncompleteItem = cJSON_GetObjectItem(json, "isIncomplete");
  list->isIncomplete = !(isIncompleteItem == NULL || cJSON_IsFalse(isIncompleteItem));

  LSP_getCompletionArrayFromJSON(cJSON_GetObjectItem(json, "items"), &list->completions);
}


void LSP_getCompletionArrayFromJSON(cJSON* json, LSP_CompletionArray* array) {
  array->items = NULL;
  array->size = 0;

  if (json == NULL) {
    return;
  }

  array->size = cJSON_GetArraySize(json);
  array->items = malloc(array->size * sizeof(LSP_CompletionItem));
  for (int i = 0; i < array->size; i++) {
    LSP_getCompletionItemFromJSON(cJSON_GetArrayItem(json, i), array->items + i);
  }
}


void LSP_getCompletionItemFromJSON(cJSON* json, LSP_CompletionItem* item) {
  assert(item != NULL);

  // defaults
  item->detail[0] = '\0';
  item->description[0] = '\0';
  item->kind = ct_Text;
  item->documentation[0] = '\0';
  item->documentationType = dt_PLAIN_TEXT;
  item->is_text_edit = false;
  item->additionalTextEdits = NULL;
  item->additionalTextEditsSize = 0;

  // copy the label
  assert(cJSON_GetObjectItem(json, "label") != NULL);
  snprintf(item->label, METHOD_MAX_LENGTH, "%s", cJSON_GetStringValue(cJSON_GetObjectItem(json, "label")));

  // fill if present labelDetails
  cJSON* tmp_item;
  if ((tmp_item = cJSON_GetObjectItem(json, "labelDetails")) != NULL) {
    cJSON* tmp_item_2;
    if ((tmp_item_2 = cJSON_GetObjectItem(tmp_item, "detail")) != NULL) {
      snprintf(item->detail, METHOD_MAX_LENGTH, "%s", cJSON_GetStringValue(tmp_item_2));
    }
    if ((tmp_item_2 = cJSON_GetObjectItem(tmp_item, "description")) != NULL) {
      snprintf(item->description, METHOD_MAX_LENGTH, "%s", cJSON_GetStringValue(tmp_item_2));
    }
  }

  // fill if present detail
  if ((tmp_item = cJSON_GetObjectItem(tmp_item, "detail")) != NULL) {
    snprintf(item->detail, METHOD_MAX_LENGTH, "%s", cJSON_GetStringValue(tmp_item));
  }

  // fill if present kind
  if ((tmp_item = cJSON_GetObjectItem(json, "kind")) != NULL) {
    item->kind = cJSON_GetNumberValue(tmp_item);
  }

  // fill if present the documentation
  if ((tmp_item = cJSON_GetObjectItem(json, "documentation"))) {
    if (cJSON_IsString(tmp_item)) {
      snprintf(item->documentation, MESSAGE_LENGTH, "%s", cJSON_GetStringValue(tmp_item));
    }
    else {
      item->documentationType = cJSON_GetNumberValue(cJSON_GetObjectItem(tmp_item, "kind"));
      snprintf(item->documentation, MESSAGE_LENGTH, "%s", cJSON_GetStringValue(cJSON_GetObjectItem(tmp_item, "value")));
    }
  }

  // fill if present sortText
  if ((tmp_item = cJSON_GetObjectItem(json, "sortText"))) {
    snprintf(item->sortText, METHOD_MAX_LENGTH, "%s", cJSON_GetStringValue(tmp_item));
  }
  else {
    strncpy(item->sortText, item->label, METHOD_MAX_LENGTH);
  }

  // fill if present filterText
  if ((tmp_item = cJSON_GetObjectItem(json, "filterText"))) {
    snprintf(item->filterText, METHOD_MAX_LENGTH, "%s", cJSON_GetStringValue(tmp_item));
  }
  else {
    strncpy(item->filterText, item->label, METHOD_MAX_LENGTH);
  }

  // fill if present insertText
  if ((tmp_item = cJSON_GetObjectItem(json, "insertText"))) {
    snprintf(item->insertText, METHOD_MAX_LENGTH, "%s", cJSON_GetStringValue(tmp_item));
  }
  else {
    strncpy(item->insertText, item->label, METHOD_MAX_LENGTH);
  }

  // fill if present textEdit
  if ((tmp_item = cJSON_GetObjectItem(json, "textEdit"))) {
    item->text_edit = LSP_getTextEditFromJSON(tmp_item);
    item->is_text_edit = true;
  }

  if ((tmp_item = cJSON_GetObjectItem(json, "additionalTextEdits"))) {
    item->additionalTextEditsSize = cJSON_GetArraySize(tmp_item);
    item->additionalTextEdits = malloc(item->additionalTextEditsSize * sizeof(LSP_TextEdit));
    for (int i = 0; i < item->additionalTextEditsSize; i++) {
      item->additionalTextEdits[i] = LSP_getTextEditFromJSON(cJSON_GetArrayItem(tmp_item, i));
    }
  }
}

void LSP_destroyCompletionItem(LSP_CompletionItem* item) {
  if (item->is_text_edit) {
    LSP_destroyTextEdit(item->text_edit);
  }
  for (int j = 0; j < item->additionalTextEditsSize; j++) {
    LSP_destroyTextEdit(item->additionalTextEdits[j]);
  }
  free(item->additionalTextEdits);
  item->additionalTextEdits = NULL;
  item->additionalTextEditsSize = 0;
}


void LSP_destroyCompletionList(LSP_CompletionList* completion_list) {
  if (!completion_list) {
    return;
  }
  for (int i = 0; i < completion_list->completions.size; i++) {
    LSP_destroyCompletionItem(completion_list->completions.items + i);
  }
  free(completion_list->completions.items);

  // defaults
  completion_list->isIncomplete = false;
  completion_list->completions.items = NULL;
  completion_list->completions.size = 0;
}


void LSP_getHoverFromJSON(cJSON* json, LSP_Hover* hover_list) {
  if (!json || cJSON_IsNull(json)) {
    hover_list->size = 0;
    hover_list->contents = NULL;
    hover_list->is_range = false;
    return;
  }

  cJSON* range = cJSON_GetObjectItem(json, "range");
  if (range) {
    hover_list->range = LSP_getRangeFromJSON(range);
    hover_list->is_range = true;
  }
  else {
    hover_list->is_range = false;
  }

  cJSON* contents = cJSON_GetObjectItem(json, "contents");
  if (cJSON_IsArray(contents)) {
    int array_size = cJSON_GetArraySize(contents);
    hover_list->contents = malloc(sizeof(LSP_MarkedString) * array_size);
    int valid_count = 0;
    for (int i = 0; i < array_size; i++) {
      LSP_getMarkedStringFromJSON(cJSON_GetArrayItem(contents, i), &hover_list->contents[valid_count]);
      if (hover_list->contents[valid_count].value[0] != '\0') {
        valid_count++;
      }
    }
    hover_list->size = valid_count;
  }
  else {
    hover_list->size = 1;
    hover_list->contents = malloc(sizeof(LSP_MarkedString));
    LSP_getMarkedStringFromJSON(contents, &hover_list->contents[0]);
    if (hover_list->contents[0].value[0] == '\0') {
      hover_list->size = 0;
    }
  }
}

void LSP_getMarkedStringFromJSON(cJSON* json, LSP_MarkedString* item) {
  item->value[0] = '\0';
  item->documentationType = dt_PLAIN_TEXT;

  if (cJSON_IsString(json)) {
    snprintf(item->value, MESSAGE_LENGTH, "%s", cJSON_GetStringValue(json));
  }
  else if (cJSON_IsObject(json)) {
    cJSON* kind = cJSON_GetObjectItem(json, "kind");
    cJSON* value = cJSON_GetObjectItem(json, "value");
    if (kind && value) {
      // MarkupContent
      if (strcmp(cJSON_GetStringValue(kind), "markdown") == 0) {
        item->documentationType = dt_MARKDOWN;
      }
      snprintf(item->value, MESSAGE_LENGTH, "%s", cJSON_GetStringValue(value));
    }
    else if (value) {
      // MarkedString with { language, value }

      /* TODO handle MarkedString as a markdown documentationType with
       *  * The pair of a language and a value is an equivalent to markdown:
       *  ```${language}
       *  ${value}
       *  ```
       */
      snprintf(item->value, MESSAGE_LENGTH, "%s", cJSON_GetStringValue(value));
    }
  }
}

void LSP_destroyHover(LSP_Hover* hover_list) {
  if (!hover_list) {
    return;
  }
  if (hover_list->contents) {
    free(hover_list->contents);
    hover_list->contents = NULL;
  }
  hover_list->size = 0;
  hover_list->is_range = false;
}

void LSP_getSignatureHelpFromJSON(cJSON* json, LSP_SignatureHelp* help) {
  if (!json) {
    return;
  }
  help->activeSignature = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(json, "activeSignature"));
  help->activeParameter = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(json, "activeParameter"));

  cJSON* signatures_json = cJSON_GetObjectItem(json, "signatures");
  help->signatures_size = cJSON_GetArraySize(signatures_json);
  help->signatures = malloc(sizeof(LSP_SignatureInformation) * help->signatures_size);

  for (int i = 0; i < help->signatures_size; i++) {
    cJSON* sig_json = cJSON_GetArrayItem(signatures_json, i);
    LSP_SignatureInformation* sig = &help->signatures[i];

    sig->label[0] = '\0';
    sig->documentation[0] = '\0';

    cJSON* label = cJSON_GetObjectItem(sig_json, "label");
    if (cJSON_IsString(label)) {
      strncpy(sig->label, label->valuestring, MESSAGE_LENGTH - 1);
    }

    cJSON* doc = cJSON_GetObjectItem(sig_json, "documentation");
    if (cJSON_IsString(doc)) {
      strncpy(sig->documentation, doc->valuestring, MESSAGE_LENGTH - 1);
    }
    else if (cJSON_IsObject(doc)) {
      strncpy(sig->documentation, cJSON_GetStringValue(cJSON_GetObjectItem(doc, "value")), MESSAGE_LENGTH - 1);
    }

    cJSON* params_json = cJSON_GetObjectItem(sig_json, "parameters");
    sig->parameters_size = cJSON_GetArraySize(params_json);
    sig->parameters = malloc(sizeof(LSP_ParameterInformation) * sig->parameters_size);

    for (int j = 0; j < sig->parameters_size; j++) {
      cJSON* param_json = cJSON_GetArrayItem(params_json, j);
      LSP_ParameterInformation* param = &sig->parameters[j];
      param->label[0] = '\0';
      param->documentation[0] = '\0';
      param->start = -1;
      param->end = -1;

      cJSON* p_label = cJSON_GetObjectItem(param_json, "label");
      if (cJSON_IsString(p_label)) {
        strncpy(param->label, p_label->valuestring, MESSAGE_LENGTH - 1);
      }
      else if (cJSON_IsArray(p_label) && cJSON_GetArraySize(p_label) == 2) {
        param->start = (int)cJSON_GetNumberValue(cJSON_GetArrayItem(p_label, 0));
        param->end = (int)cJSON_GetNumberValue(cJSON_GetArrayItem(p_label, 1));

        // If offsets are provided, we can extract the label from the signature label
        if (param->start >= 0 && param->end > param->start && param->end <= (int)strlen(sig->label)) {
          int len = param->end - param->start;
          if (len >= MESSAGE_LENGTH) {
            len = MESSAGE_LENGTH - 1;
          }
          strncpy(param->label, sig->label + param->start, len);
          param->label[len] = '\0';
        }
      }

      cJSON* p_doc = cJSON_GetObjectItem(param_json, "documentation");
      if (cJSON_IsString(p_doc)) {
        strncpy(param->documentation, p_doc->valuestring, MESSAGE_LENGTH - 1);
      }
      else if (cJSON_IsObject(p_doc)) {
        strncpy(param->documentation, cJSON_GetStringValue(cJSON_GetObjectItem(p_doc, "value")), MESSAGE_LENGTH - 1);
      }
    }

    cJSON* active_param = cJSON_GetObjectItem(sig_json, "activeParameter");
    if (active_param) {
      sig->activeParameter = (int)cJSON_GetNumberValue(active_param);
    }
    else {
      sig->activeParameter = -1;
    }
  }
}

void LSP_destroySignatureHelp(LSP_SignatureHelp* help) {
  if (!help) {
    return;
  }
  for (int i = 0; i < help->signatures_size; i++) {
    free(help->signatures[i].parameters);
  }
  free(help->signatures);
  help->signatures = NULL;
  help->signatures_size = 0;
}


//// -------- Receive Functions --------

bool LSP_dispatchOnReceive(LSP_Server* lsp, void (*dispatcher)(cJSON* packet, LSP_Server* lsp, void* payload),
                           void* payload) {
  if (lsp == NULL || lsp->pid == -1 || !lsp->is_init_done) {
    return false;
  }

  cJSON* packet = LSP_readPacketAsJSON(lsp, false);
  if (packet == NULL) {
    return false;
  }
  if (dispatcher != NULL) {
    dispatcher(packet, lsp, payload);
  }
  cJSON_Delete(packet);

  return true;
}


//// -------- Send Functions --------


void LSP_notifyLspFileDidOpen(LSP_Server* lsp, char* file_name, char* file_content) {
  cJSON* request_content = cJSON_CreateObject();

  cJSON* text_document = LSP_getJSONTextDocumentItem(file_name, lsp->language, 1, file_content);
  cJSON_AddItemToObject(request_content, "textDocument", text_document);


  LSP_sendPacketWithJSON(lsp, "textDocument/didOpen", request_content, LSP_NOTIFICATION);

  cJSON_Delete(request_content);
}


void LSP_notifyLspFileDidClose(LSP_Server* lsp, char* file_name) {
  cJSON* request_content = cJSON_CreateObject();

  cJSON* text_document = LSP_getJSONTextDocumentIdentifier(file_name);
  cJSON_AddItemToObject(request_content, "textDocument", text_document);


  LSP_sendPacketWithJSON(lsp, "textDocument/didClose", request_content, LSP_NOTIFICATION);

  cJSON_Delete(request_content);
}


void LSP_notifyLspFileDidChange(LSP_Server* lsp, char* file_name, cJSON* array_of_changes, int version) {
  cJSON* request_content = cJSON_CreateObject();

  cJSON* text_document = LSP_getJSONTextDocumentIdentifierVersionned(file_name, version);
  cJSON_AddItemToObject(request_content, "textDocument", text_document);
  cJSON_AddItemToObject(request_content, "contentChanges", array_of_changes);

  LSP_sendPacketWithJSON(lsp, "textDocument/didChange", request_content, LSP_NOTIFICATION);

  cJSON_Delete(request_content);
}


void LSP_requestCompletion(LSP_Server* lsp, char* file_name, LSP_Position pos) {
  cJSON* request_content = cJSON_CreateObject();

  cJSON* text_document = LSP_getJSONTextDocumentIdentifier(file_name);
  cJSON_AddItemToObject(request_content, "textDocument", text_document);

  cJSON* position = LSP_getJSONPosition(pos);
  cJSON_AddItemToObject(request_content, "position", position);

  LSP_PacketID id = LSP_sendPacketWithJSON(lsp, "textDocument/completion", request_content, LSP_REQUEST);

  LSP_addResponseContext(lsp, id, "textDocument/completion", file_name, NULL);

  cJSON_Delete(request_content);
}


void LSP_requestHover(LSP_Server* lsp, char* file_name, LSP_Position pos) {
  cJSON* request_content = cJSON_CreateObject();

  cJSON* text_document = LSP_getJSONTextDocumentIdentifier(file_name);
  cJSON_AddItemToObject(request_content, "textDocument", text_document);

  cJSON* position_json = LSP_getJSONPosition(pos);
  cJSON_AddItemToObject(request_content, "position", position_json);

  LSP_PacketID id = LSP_sendPacketWithJSON(lsp, "textDocument/hover", request_content, LSP_REQUEST);

  LSP_Position* p = malloc(sizeof(LSP_Position));
  *p = pos;

  LSP_addResponseContext(lsp, id, "textDocument/hover", file_name, p);

  cJSON_Delete(request_content);
}


char* LSP_getMethodForGotoType(LSP_GOTO_TYPE goto_type) {
  switch (goto_type) {
    case LSP_GOTO_DECLARATION:
      return "textDocument/declaration";
    case LSP_GOTO_DEFINITION:
      return "textDocument/definition";
    case LSP_GOTO_TYPE_DEFINITION:
      return "textDocument/typeDefinition";
    case LSP_GOTO_IMPLEMENTATION:
      return "textDocument/implementation";
    case LSP_FIND_REFERENCE:
      return "textDocument/references";
  }
  assert(false);
}

void LSP_requestGoto(LSP_Server* lsp, char* file_name, LSP_Position pos, LSP_GOTO_TYPE goto_type) {
  char* goto_method = LSP_getMethodForGotoType(goto_type);

  cJSON* request_content = cJSON_CreateObject();

  cJSON* text_document = LSP_getJSONTextDocumentIdentifier(file_name);
  cJSON_AddItemToObject(request_content, "textDocument", text_document);

  cJSON* position_json = LSP_getJSONPosition(pos);
  cJSON_AddItemToObject(request_content, "position", position_json);

  // little tweakfor find_reference which is not strictly following the same API
  if (goto_type == LSP_FIND_REFERENCE) {
    cJSON* context = cJSON_CreateObject();
    cJSON_AddBoolToObject(context, "includeDeclaration", true);
    cJSON_AddItemToObject(request_content, "context", context);
  }

  LSP_PacketID id = LSP_sendPacketWithJSON(lsp, goto_method, request_content, LSP_REQUEST);

  GotoContext* goto_payload = malloc(sizeof(GotoContext));
  goto_payload->pos = pos;
  goto_payload->goto_type = goto_type;

  LSP_addResponseContext(lsp, id, goto_method, file_name, goto_payload);

  cJSON_Delete(request_content);
}

void LSP_requestFormatting(LSP_Server* lsp, char* file_name, LSP_FormattingOptions options) {
  cJSON* request_content = cJSON_CreateObject();

  cJSON* text_document = LSP_getJSONTextDocumentIdentifier(file_name);
  cJSON_AddItemToObject(request_content, "textDocument", text_document);

  cJSON* formatting_options = cJSON_CreateObject();
  cJSON_AddNumberToObject(formatting_options, "tabSize", options.tabSize);
  cJSON_AddBoolToObject(formatting_options, "insertSpaces", options.insertSpaces);
  cJSON_AddBoolToObject(formatting_options, "trimTrailingWhitespace", options.trimTrailingWhitespace);
  cJSON_AddBoolToObject(formatting_options, "insertFinalNewline", options.insertFinalNewline);
  cJSON_AddBoolToObject(formatting_options, "trimFinalNewlines", options.trimFinalNewlines);

  cJSON_AddItemToObject(request_content, "options", formatting_options);

  LSP_PacketID id = LSP_sendPacketWithJSON(lsp, "textDocument/formatting", request_content, LSP_REQUEST);

  LSP_addResponseContext(lsp, id, "textDocument/formatting", file_name, NULL);

  cJSON_Delete(request_content);
}

void LSP_requestOnTypeFormatting(LSP_Server* lsp, char* file_name, LSP_Position pos, char* ch,
                                 LSP_FormattingOptions options) {
  cJSON* request_content = cJSON_CreateObject();

  cJSON* text_document = LSP_getJSONTextDocumentIdentifier(file_name);
  cJSON_AddItemToObject(request_content, "textDocument", text_document);

  cJSON_AddItemToObject(request_content, "position", LSP_getJSONPosition(pos));
  cJSON_AddStringToObject(request_content, "ch", ch);

  cJSON* formatting_options = cJSON_CreateObject();
  cJSON_AddNumberToObject(formatting_options, "tabSize", options.tabSize);
  cJSON_AddBoolToObject(formatting_options, "insertSpaces", options.insertSpaces);
  cJSON_AddBoolToObject(formatting_options, "trimTrailingWhitespace", options.trimTrailingWhitespace);
  cJSON_AddBoolToObject(formatting_options, "insertFinalNewline", options.insertFinalNewline);
  cJSON_AddBoolToObject(formatting_options, "trimFinalNewlines", options.trimFinalNewlines);

  cJSON_AddItemToObject(request_content, "options", formatting_options);

  LSP_PacketID id = LSP_sendPacketWithJSON(lsp, "textDocument/onTypeFormatting", request_content, LSP_REQUEST);

  LSP_addResponseContext(lsp, id, "textDocument/onTypeFormatting", file_name, NULL);

  cJSON_Delete(request_content);
}

void LSP_requestSignatureHelp(LSP_Server* lsp, char* file_name, LSP_Position pos) {
  char* method = "textDocument/signatureHelp";
  cJSON* params = LSP_getJSONTextDocumentPositionParams(file_name, pos);

  LSP_PacketID id = LSP_sendPacketWithJSON(lsp, method, params, LSP_REQUEST);

  LSP_addResponseContext(lsp, id, method, file_name, NULL);

  cJSON_Delete(params);
}

cJSON* LSP_getJSONDiagnosticFull(LSP_Diagnostic* diag) {
  cJSON* diagnostic = cJSON_CreateObject();
  cJSON_AddItemToObject(diagnostic, "range", LSP_getJSONRange(diag->range));
  cJSON_AddNumberToObject(diagnostic, "severity", diag->severity);
  if (diag->code[0]) {
    cJSON_AddStringToObject(diagnostic, "code", diag->code);
  }
  if (diag->source[0]) {
    cJSON_AddStringToObject(diagnostic, "source", diag->source);
  }
  cJSON_AddStringToObject(diagnostic, "message", diag->message);
  return diagnostic;
}

void LSP_requestCodeAction(LSP_Server* lsp, char* file_name, LSP_Range range, LSP_Diagnostic* diagnostics,
                           int diagnostics_count) {
  cJSON* request_content = cJSON_CreateObject();
  cJSON_AddItemToObject(request_content, "textDocument", LSP_getJSONTextDocumentIdentifier(file_name));
  cJSON_AddItemToObject(request_content, "range", LSP_getJSONRange(range));

  cJSON* context = cJSON_AddObjectToObject(request_content, "context");
  cJSON* diags_array = cJSON_AddArrayToObject(context, "diagnostics");
  for (int i = 0; i < diagnostics_count; i++) {
    cJSON_AddItemToArray(diags_array, LSP_getJSONDiagnosticFull(&diagnostics[i]));
  }

  LSP_PacketID id = LSP_sendPacketWithJSON(lsp, "textDocument/codeAction", request_content, LSP_REQUEST);
  LSP_addResponseContext(lsp, id, "textDocument/codeAction", file_name, NULL);

  cJSON_Delete(request_content);
}
