/*
 * Partial implementation of :
 * https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/
 *
 * !! WARNING !!
 *
 * Note that all LSP_* structure are (0-based for row and column) and that the Cursor api is (1-based row, and 0-based
 * column)
 *
 */
#ifndef CLIENT_H
#define CLIENT_H

#include <linux/limits.h>
#include <stdbool.h>
#include <sys/types.h>

#include "../../../lib/cJSON/cJSON.h"

#define MESSAGE_LENGTH     4092
#define METHOD_MAX_LENGTH  200
#define LANGUAGE_ID_LENGTH 100

typedef enum { LSP_REQUEST, LSP_NOTIFICATION, LSP_RESPONSE } LSP_PACKET_TYPE;

typedef unsigned long LSP_PacketID;

struct _LSP_ResponseContext {
  LSP_PacketID id;
  char* method;
  char file_name[PATH_MAX];
  void* payload;
  struct _LSP_ResponseContext* next;
};

typedef struct _LSP_ResponseContext LSP_ResponseContext;

struct _LSP_PendingPacket {
  char* method;
  char* params;
  LSP_PACKET_TYPE type;
  LSP_PacketID id;
  struct _LSP_PendingPacket* next;
};

typedef struct _LSP_PendingPacket LSP_PendingPacket;

typedef enum {
  LSP_POSITION_ENCODING_UTF16,
  LSP_POSITION_ENCODING_UTF8,
  LSP_POSITION_ENCODING_UTF32
} LSP_PositionEncoding;

typedef struct {
  char name[LANGUAGE_ID_LENGTH];
  char language[LANGUAGE_ID_LENGTH];
  pid_t pid;
  int inpipefd[2];
  int outpipefd[2];
  cJSON* init_result;
  LSP_PacketID request_id;
  LSP_ResponseContext* response_contexts;

  // Internal wait
  pthread_mutex_t initDone;
  bool is_init_done;
  LSP_PendingPacket* pending_packets;
  pthread_mutex_t pending_lock;

  // Capabilities
  LSP_PositionEncoding position_encoding;
  char** on_type_trigger_chars;
  int on_type_trigger_chars_count;
} LSP_Server;


//////// ---------------- JSON TOOLS ---------------------

void printToStdioJSON(cJSON* json);


//////// --------------- Low Layer JSON-RPC --------------------------

bool LSP_openLSPServer(char* name, char* command_args, char* language, LSP_Server* server);

void LSP_closeLSPServer(LSP_Server* server);

char* LSP_readPacket(LSP_Server* server);

cJSON* LSP_readPacketAsJSON(LSP_Server* server, bool block);

int LSP_sendPacket(LSP_Server* server, char* method, char* params, LSP_PACKET_TYPE type);

int LSP_sendPacketWithJSON(LSP_Server* server, char* method, cJSON* content, LSP_PACKET_TYPE type);

LSP_PACKET_TYPE LSP_getPacketType(cJSON* content);

void LSP_addResponseContext(LSP_Server* server, LSP_PacketID id, char* method, char* file_name, void* payload);
bool LSP_popResponseContext(LSP_Server* server, LSP_PacketID id, LSP_ResponseContext* context);
bool LSP_hasPendingRequest(LSP_Server* server, char* method);
void LSP_clearResponseContext(LSP_Server* server);

//////// ----------------- Created Functions ---------------

//// -------- Tools Functions --------

void LSP_initializeServer(LSP_Server* lsp, char* client_name, char* client_version, char* current_workspace);

cJSON* LSP_extractPacketResult(cJSON* response_obj);

LSP_PacketID LSP_getPacketID(cJSON* request_body);

char* LSP_getPacketMethod(cJSON* request_body);

cJSON* LSP_getPacketResult(cJSON* request_body);

cJSON* LSP_getNotificationParams(cJSON* notification_body);

//// -------- JSON conversion --------*

typedef enum {
  LSP_GOTO_DECLARATION,
  LSP_GOTO_DEFINITION,
  LSP_GOTO_TYPE_DEFINITION,
  LSP_GOTO_IMPLEMENTATION,
  LSP_FIND_REFERENCE
} LSP_GOTO_TYPE;

typedef struct {
  int row;    // 0-based
  int column; // 0-based
} LSP_Position;

typedef struct {
  LSP_Position pos1;
  LSP_Position pos2;
} LSP_Range;


typedef struct {
  LSP_Position pos;
  LSP_GOTO_TYPE goto_type;
} GotoContext;

LSP_Position LSP_pos(int lsp_row, int lsp_col);
LSP_Position LSP_getPositionFromJSON(cJSON* json);
cJSON* LSP_getJSONPosition(LSP_Position pos);

LSP_Range LSP_range(LSP_Position p1, LSP_Position p2);
LSP_Range LSP_getRangeFromJSON(cJSON* json);
cJSON* LSP_getJSONRange(LSP_Range range);

typedef struct {
  char file_name[PATH_MAX];
  char languageId[LANGUAGE_ID_LENGTH];
  int version;
  char* text;
} LSP_TextDocumentItem;

LSP_TextDocumentItem LSP_getTextDocumentItemOf(char* file_name, char* languageId, int version, char* text);
cJSON* LSP_getJSONTextDocumentItem(char* file_name, char* languageId, int version, char* text);
LSP_TextDocumentItem LSP_getTextDocumentItemFromJSON(cJSON* json);
void LSP_destroyTextDocumentItem(LSP_TextDocumentItem text_document_item);

typedef struct {
  char file_name[PATH_MAX];
} LSP_TextDocumentIdentifier;

LSP_TextDocumentIdentifier LSP_getTextDocumentIdentifierOf(char* file_name);
cJSON* LSP_getJSONTextDocumentIdentifier(char* file_name);
LSP_TextDocumentIdentifier LSP_getTextDocumentIdentifierFromJSON(cJSON* json);
void LSP_destroyTextDocumentIdentifier(LSP_TextDocumentIdentifier* text_document_identifier);

cJSON* LSP_getJSONTextDocumentIdentifierVersionned(char* file_name, int version);

typedef struct {
  LSP_TextDocumentIdentifier text_id;
  LSP_Position position;
} LSP_TextDocumentPositionParams;

LSP_TextDocumentPositionParams LSP_getTextDocumentPositionParamsOf(char* file_name, LSP_Position pos);
cJSON* LSP_getJSONTextDocumentPositionParams(char* file_name, LSP_Position pos);
LSP_TextDocumentPositionParams LSP_getTextDocumentPositionParamsFromJSON(cJSON* json);
void LSP_destroyTextDocumentPositionParams(LSP_TextDocumentPositionParams text_document_position_params);

typedef struct {
  LSP_Range range;
  char* new_text;
} LSP_TextEdit;

LSP_TextEdit LSP_getTextEditOf(LSP_Range range, char* new_text);
cJSON* LSP_getJSONTextEdit(LSP_Range range, char* new_text);
LSP_TextEdit LSP_getTextEditFromJSON(cJSON* json);
void LSP_destroyTextEdit(LSP_TextEdit text_edit);

typedef struct {
  LSP_TextDocumentIdentifier text_document;
  LSP_TextEdit* edits;
  int edits_count;
} LSP_TextDocumentEdit;

LSP_TextDocumentEdit LSP_getTextDocumentEditFromJSON(cJSON* json);
void LSP_destroyTextDocumentEdit(LSP_TextDocumentEdit* text_document_edit);

typedef struct {
  LSP_TextDocumentEdit* document_changes;
  int document_changes_count;
} LSP_WorkspaceEdit;

LSP_WorkspaceEdit LSP_getWorkspaceEditFromJSON(cJSON* json);
void LSP_destroyWorkspaceEdit(LSP_WorkspaceEdit* workspace_edit);

typedef struct {
  char title[200];
  char command[100];
  cJSON* arguments;
} LSP_Command;

typedef struct {
  char title[200];
  char kind[50];
  bool isPreferred;
  LSP_WorkspaceEdit edit;
  LSP_Command command;
  bool has_edit;
  bool has_command;
} LSP_CodeAction;

LSP_CodeAction LSP_getCodeActionFromJSON(cJSON* json);
void LSP_destroyCodeAction(LSP_CodeAction* code_action);

void LSP_sendResponse(LSP_Server* server, LSP_PacketID id, cJSON* result);

void LSP_requestExecuteCommand(LSP_Server* lsp, char* file_name, LSP_Command* command);

typedef struct {
  LSP_TextDocumentIdentifier file_name;
  LSP_Range range;
} LSP_Location;

LSP_Location LSP_getLocationOf(char* file_name, LSP_Range range);
cJSON* LSP_getJSONLocation(char* file_name, LSP_Range range);
LSP_Location LSP_getLocationFromJSON(cJSON* json);
void LSP_destroyLocation(LSP_Location* location);

typedef struct {
  LSP_Location* items;
  int size;
} LSP_LocationArray;

void LSP_getLocationArrayFromJSON(cJSON* json, LSP_LocationArray* array);
void LSP_destroyLocationArray(LSP_LocationArray* array);

typedef struct {
  LSP_Location location;
  char message[MESSAGE_LENGTH];
} LSP_DiagnosticRelatedInformation;

// TODO implement if needed
LSP_DiagnosticRelatedInformation LSP_getDiagnosticRelatedInformationOf(char* file_name, LSP_Range range);
cJSON* LSP_getJSONDiagnosticRelatedInformation(char* file_name, LSP_Range range);
LSP_DiagnosticRelatedInformation LSP_getDiagnosticRelatedInformationFromJSON(cJSON* json);
void LSP_destroyDiagnosticRelatedInformation(LSP_DiagnosticRelatedInformation location);

typedef enum {
  LSP_DIAG_ERROR = 1,
  LSP_DIAG_WARNING = 2,
  LSP_DIAG_INFORMATION = 3,
  LSP_DIAG_HINT = 4,
  LSP_DIAG_SEVERITY_NONE = 0
} LSP_DiagnosticSeverity;
typedef enum { LSP_DIAG_UNNECESSARY = 1, LSP_DIAG_DEPRECATED = 2, LSP_DIAG_TAG_NONE = 0 } LSP_DiagnosticTag;
typedef struct {
  LSP_DiagnosticSeverity severity;
  LSP_Range range;
  char source[100];
  char code[MESSAGE_LENGTH];
  char message[MESSAGE_LENGTH];
  char codeDescription[MESSAGE_LENGTH];
  // TODO implement if needed
  LSP_DiagnosticTag tags[100];
  // TODO implement if needed
  LSP_DiagnosticRelatedInformation infos[0];
} LSP_Diagnostic;

LSP_Diagnostic LSP_getDiagnosticOf(LSP_Range range);
cJSON* LSP_getJSONDiagnostic(LSP_Range range);
LSP_Diagnostic LSP_getDiagnosticFromJSON(cJSON* json);
void LSP_destroyDiagnostic(LSP_Diagnostic* diagnostic);

typedef struct {
  LSP_Diagnostic* items;
  int size;
} LSP_DiagnosticList;

void LSP_initDiagnosticList(LSP_DiagnosticList* list);
void LSP_destroyDiagnosticList(LSP_DiagnosticList* list);

typedef struct {
  LSP_CodeAction* items;
  int size;
} LSP_CodeActionList;

void LSP_initCodeActionList(LSP_CodeActionList* list);
void LSP_destroyCodeActionList(LSP_CodeActionList* list);


typedef enum {
  dt_PLAIN_TEXT,
  dt_MARKDOWN,
} LSP_DocumentationType;

typedef enum {
  ct_Text = 1,
  ct_Method = 2,
  ct_Function = 3,
  ct_Constructor = 4,
  ct_Field = 5,
  ct_Variable = 6,
  ct_Class = 7,
  ct_Interface = 8,
  ct_Module = 9,
  ct_Property = 10,
  ct_Unit = 11,
  ct_Value = 12,
  ct_Enum = 13,
  ct_Keyword = 14,
  ct_Snippet = 15,
  ct_Color = 16,
  ct_File = 17,
  ct_Reference = 18,
  ct_Folder = 19,
  ct_EnumMember = 20,
  ct_Constant = 21,
  ct_Struct = 22,
  ct_Event = 23,
  ct_Operator = 24,
  ct_TypeParameter = 25,
} LSP_CompletionType;

typedef enum { citf_PlainText = 1, citf_Snippet = 2 } LSP_CompletionInsertTextFormat;


struct _CompletionItem {
  char label[METHOD_MAX_LENGTH];
  char detail[METHOD_MAX_LENGTH];
  char description[MESSAGE_LENGTH];
  LSP_CompletionType kind;
  char documentation[MESSAGE_LENGTH];
  LSP_DocumentationType documentationType;
  char sortText[METHOD_MAX_LENGTH];
  char filterText[METHOD_MAX_LENGTH];
  char insertText[METHOD_MAX_LENGTH];
  LSP_TextEdit text_edit;
  bool is_text_edit;
  LSP_TextEdit* additionalTextEdits;
  int additionalTextEditsSize;
};

typedef struct _CompletionItem LSP_CompletionItem;

typedef struct {
  int size;
  LSP_CompletionItem* items;
} LSP_CompletionArray;

typedef struct {
  bool isIncomplete;
  LSP_CompletionArray completions;
} LSP_CompletionList;

void LSP_getCompletionListFromJSON(cJSON* json, LSP_CompletionList* list);
void LSP_getCompletionArrayFromJSON(cJSON* json, LSP_CompletionArray* array);
void LSP_getCompletionItemFromJSON(cJSON* json, LSP_CompletionItem* item);
void LSP_destroyCompletionItem(LSP_CompletionItem* item);
void LSP_destroyCompletionList(LSP_CompletionList* completion_list);


typedef struct MarkedString {
  char value[MESSAGE_LENGTH];
  LSP_DocumentationType documentationType;
} LSP_MarkedString;

typedef struct {
  LSP_MarkedString* contents;
  int size;
  bool is_range;
  LSP_Range range;
} LSP_Hover;

typedef struct {
  char label[MESSAGE_LENGTH];
  int start;
  int end;
  char documentation[MESSAGE_LENGTH];
} LSP_ParameterInformation;

typedef struct {
  char label[MESSAGE_LENGTH];
  char documentation[MESSAGE_LENGTH];
  LSP_ParameterInformation* parameters;
  int parameters_size;
  int activeParameter;
} LSP_SignatureInformation;

typedef struct {
  LSP_SignatureInformation* signatures;
  int signatures_size;
  int activeSignature;
  int activeParameter;
} LSP_SignatureHelp;

typedef struct {
  int tabSize;
  bool insertSpaces;
  bool trimTrailingWhitespace;
  bool insertFinalNewline;
  bool trimFinalNewlines;
} LSP_FormattingOptions;

void LSP_getHoverFromJSON(cJSON* json, LSP_Hover* hover_list);
void LSP_getMarkedStringFromJSON(cJSON* json, LSP_MarkedString* item);
void LSP_destroyHover(LSP_Hover* hover_list);

void LSP_getSignatureHelpFromJSON(cJSON* json, LSP_SignatureHelp* help);
void LSP_destroySignatureHelp(LSP_SignatureHelp* help);


//// -------- Receive Functions --------

bool LSP_dispatchOnReceive(LSP_Server* lsp, void (*dispatcher)(cJSON* packet, LSP_Server* lsp, void* payload),
                           void* payload);


//// -------- Notify Functions --------


void LSP_notifyLspFileDidOpen(LSP_Server* lsp, char* file_name, char* file_content);
void LSP_notifyLspFileDidClose(LSP_Server* lsp, char* file_name);
void LSP_notifyLspFileDidChange(LSP_Server* lsp, char* file_name, cJSON* array_of_changes, int version);


//// -------- Request Functions --------


void LSP_requestCompletion(LSP_Server* lsp, char* file_name, LSP_Position pos);
void LSP_requestHover(LSP_Server* lsp, char* file_name, LSP_Position pos);
void LSP_requestGoto(LSP_Server* lsp, char* file_name, LSP_Position pos, LSP_GOTO_TYPE goto_type);
void LSP_requestFormatting(LSP_Server* lsp, char* file_name, LSP_FormattingOptions options);
void LSP_requestOnTypeFormatting(LSP_Server* lsp, char* file_name, LSP_Position pos, char* ch,
                                 LSP_FormattingOptions options);
void LSP_requestSignatureHelp(LSP_Server* lsp, char* file_name, LSP_Position pos);

void LSP_requestCodeAction(LSP_Server* lsp, char* file_name, LSP_Range range, LSP_Diagnostic* diagnostics,
                           int diagnostics_count);

#endif // CLIENT_H
