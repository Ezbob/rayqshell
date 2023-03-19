#include "fs_commands.h"
#include "../rqshell_args.h"
#include "../rqshell.h"
#include "raylib.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

static inline bool ls_print_file(char const *path) {
  if (DirectoryExists(path)) {
    FilePathList p = LoadDirectoryFiles(path);

    for (unsigned i = 0; i < p.count; ++i) {
      rqshell_printlnf("%s", p.paths[i]);
    }

    UnloadDirectoryFiles(p);
    return true;
  } else if (FileExists(path)) {
    rqshell_println(path);
    return true;
  } else {
    rqshell_printlnf("Error: %s: no such file or directory", path);
    return false;
  }
}

void rqshell_command_pwd(int cs, char const *cc) {
  if (cs > 0) {
    rqshell_println("'pwd' does not take any arguments");
    return;
  }
  rqshell_println(GetWorkingDirectory());
}

void rqshell_command_ls(int cs, char const *cc) {
  struct rqshell_arg_iter iter = rqshell_arg_iter_init(cc, cs);
  int arg_count = rqshell_arg_iter_count_args(&iter);

  if (arg_count == 0) {
    char const *pwd = GetWorkingDirectory();
    ls_print_file(pwd);
  } else {
    const char *c = 0;
    int i = 0;
    while ((c = rqshell_arg_iter_next(&iter))) {
      if (i > 0) {
        rqshell_printlnf("===> '%s'", c);
      }
      if (!ls_print_file(c)) {
        return;
      }
      i++;
    }
  }
}

void rqshell_command_cd(int cs, char const *cc) {
  static const char *home = NULL;
  if (!home) {
    home = _strdup(GetWorkingDirectory());
  }

  struct rqshell_arg_iter iter = rqshell_arg_iter_init(cc, cs);
  int arg_count = rqshell_arg_iter_count_args(&iter);

  if (arg_count == 0) {
    return;
  } else if (arg_count > 1) {
    rqshell_println("Error: cd: too many arguments parsed to cd");
    return;
  } else if (arg_count == 1) {
    const char *c = rqshell_arg_iter_next(&iter);
    if (strcmp(c, "~") == 0) {
      rqshell_printlnf("%s", home);
      ChangeDirectory(home);
    } else if (DirectoryExists(c)) {
      rqshell_printlnf("%s", c);
      ChangeDirectory(c);
    } else {
      rqshell_printlnf("Error: %s: No such directory", c);
    }
  }
}
