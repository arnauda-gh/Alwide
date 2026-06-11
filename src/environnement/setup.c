#include "setup.h"
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../utils/logger.h"

void setupProgramEnvironnemnt() {
  signal(SIGPIPE, SIG_IGN);
#ifdef _SHOW_ERROR
  FILE* f = fopen("./.logs.txt", "w");
  if (f != NULL) {
    dup2(fileno(f), STDERR_FILENO);
    fclose(f);
  }

  setNotificationThreshold(LOG_DEBUG);
#else
  FILE* f = fopen("/dev/null", "w");
  dup2(fileno(f), STDERR_FILENO);
  fclose(f);
#endif

  setlocale(LC_ALL, "");
  setlocale(LC_NUMERIC, "C");
  system("echo > .lsp_logs.txt");
}
