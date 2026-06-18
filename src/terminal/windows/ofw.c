#include "ofw.h"

#include <libgen.h>
#include <string.h>

#include "../../data-management/file_management.h"
#include "edw.h"

void gui_initOFWContext(gui_OFW* context) {
  context->ofw = NULL;         // Opened Files Window
  context->refresh_ofw = true; // Need to reprint opened file window


  context->current_file_offset = 0;
  // Height of Opened Files Window. 0 => Disabled on start.   OPENED_FILE_WINDOW_HEIGHT => Enabled on start.
  context->ofw_height = 0;
}


void gui_resizeOFW(gui_Context* gui_context) {
  delwin(gui_context->ofw_context.ofw);
  gui_context->ofw_context.ofw = newwin(gui_context->ofw_context.ofw_height, 0, 0, gui_context->few_context.few_width);
  wbkgd(gui_context->ofw_context.ofw, COLOR_PAIR(DEFAULT_COLOR_PAIR));
  gui_context->ofw_context.refresh_ofw = true;
}


void gui_repaintOFW(gui_OFW* context, FileContainer* files, int file_count, int current_file) {
  if (!((context->refresh_ofw == true || files[current_file].io_file.status == NONE) && context->ofw_height != 0)) {
    context->refresh_ofw = false;
    return;
  }
  fprintf(stderr, "print OFW\n");
  // The current position of the cursor for the first line.
  wmove(context->ofw, 0, 0);
  int current_offset = getbegx(context->ofw);
  if (context->current_file_offset != 0) {
    current_offset += strlen("< | ");
    wattron(context->ofw, A_DIM);
    wprintw(context->ofw, "< | ");
    wattroff(context->ofw, A_DIM);
  }
  // Move to the top left corner.
  for (int i = context->current_file_offset; i < file_count; i++) {
    // Style file names.
    if (i == current_file) {
      wattron(context->ofw, A_BOLD);
    }
    else {
      wattron(context->ofw, A_DIM);
    }
    // Print file name
    char* file_name = basename(files[i].io_file.path_args);
    current_offset += strlen(file_name);
    wprintw(context->ofw, "%s", file_name);
    // Style file names.
    if (i == current_file) {
      wattroff(context->ofw, A_BOLD);
    }
    else {
      wattroff(context->ofw, A_DIM);
    }
    // Print file name separator
    if (i < file_count - 1) {
      wprintw(context->ofw, FILE_NAME_SEPARATOR);
      current_offset += strlen(FILE_NAME_SEPARATOR);
    }
    // If the file names overflow the line, print the move right text.
    if (current_offset + strlen(FILE_NAME_SEPARATOR) > COLS) {
      wmove(context->ofw, 0, COLS - getbegx(context->ofw) - strlen("... | >"));
      wattron(context->ofw, A_DIM);
      wprintw(context->ofw, "... | >");
      wattroff(context->ofw, A_DIM);
      // assert((i < argc && i < max_opened_file + 1) == false);
      break;
    }
  }
  // To erase the end of the line to avoid garbage on scroll to right.
  for (int i = current_offset; i < COLS; i++) {
    wprintw(context->ofw, " ");
  }
  // Print the bottom line.
  wmove(context->ofw, 1, 0);
  for (int i = 0; i < COLS; i++) {
    wprintw(context->ofw, "🭸");
  }

  wnoutrefresh(context->ofw);
  context->refresh_ofw = false;
}


void gui_switchOFW(gui_Context* gui_context) {
  if (gui_context->ofw_context.ofw_height == OPENED_FILE_WINDOW_HEIGHT) {
    gui_context->ofw_context.ofw_height = 0;
  }
  else {
    gui_context->ofw_context.ofw_height = OPENED_FILE_WINDOW_HEIGHT;
  }
  gui_resizeOFW(gui_context);
  gui_resizeEDW(gui_context, -1);
}
