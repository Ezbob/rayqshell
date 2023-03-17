
#include "core_commands.h"
#include "../console.h"
#include <stdlib.h>

void console_command_exit(int len, char const *c) {
  int ec = 0;
  if (len > 1) {
    console_println("Error: command 'exit' does only take one argument");
    return;
  } else if (len > 0) {
    ec = atoi(c);
  }

  exit(ec);
}

void console_command_clear(int len, char const *c) {
  if (len > 0) {
    console_println("Error: command 'clear' does not take any arguments");
    return;
  }
  console_clear();
}

void console_command_help(int len, char const *c) {
  console_println("command help:");
  console_println("    clear               : clears the text pane of text");
  console_println(
      "    exit <exit_code>    : exits the program with exit code <exit_code>");
  console_println("");
}
