#ifndef WISHWIM_EDITOR_STATE_H
#define WISHWIM_EDITOR_STATE_H

#include "editor_context.h"

void runPostProcessing(EditorContext* ctx);
void runBackgroundProcess(EditorContext* ctx, int * key);

#endif
