
#include "core_commands.h"
#include "../rqshell.h"
#include <stdlib.h>

void rqshell_command_exit(int len, char const *c) {
  int ec = 0;
  if (len > 1) {
    rqshell_println("Error: command 'exit' does only take one argument");
    return;
  } else if (len > 0) {
    ec = atoi(c);
  }

  exit(ec);
}

void rqshell_command_clear(int len, char const *c) {
  if (len > 0) {
    rqshell_println("Error: command 'clear' does not take any arguments");
    return;
  }
  rqshell_clear();
}

void rqshell_command_help(int len, char const *c) {
  rqshell_println("command help:");
  rqshell_println("    clear               : clears the text pane of text");
  rqshell_println(
      "    exit <exit_code>    : exits the program with exit code <exit_code>");
  rqshell_println("");
}
