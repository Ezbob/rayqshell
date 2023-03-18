#include "fs_commands.h"
#include "../console_args.h"
#include "../console.h"
#include "raylib.h"

static inline void ls_print_file(char const *path) {
  if (DirectoryExists(path)) {
    FilePathList p = LoadDirectoryFiles(path);

    for (unsigned i = 0; i < p.count; ++i) {
      console_printlnf("%s", p.paths[i]);
    }

    UnloadDirectoryFiles(p);
  } else if (FileExists(path)) {
    console_println(path);
  } else {
    console_printlnf("Error: %s: no such file or directory", path);
  }
}

void console_command_pwd(int cs, char const *cc) {
  if (cs > 0) {
    console_println("'pwd' does not take any arguments");
    return;
  }
  console_println(GetWorkingDirectory());
}

void console_command_ls(int cs, char const *cc) {
  struct console_arg_iter iter = console_arg_iter_init(cc, cs);
  int arg_count = console_arg_iter_count_args(&iter);

  if (arg_count == 0) {
    char const *pwd = GetWorkingDirectory();
    ls_print_file(pwd);
  } else {
    const char *c = 0;
    int i = 0;
    while ((c = console_arg_iter_next(&iter))) {
      if (i > 0) {
        console_printlnf("===> '%s'", c);
      }
      ls_print_file(c);
      i++;
    }
  }
}

void console_command_cd(int cs, char const *cc) {
  struct console_arg_iter iter = console_arg_iter_init(cc, cs);
  int arg_count = console_arg_iter_count_args(&iter);

  if (arg_count == 0) {
    return;
  } else if (arg_count > 1) {
    console_println("Error: cd: too many arguments parsed to cd");
    return;
  } else if (arg_count == 1) {
    const char *c = console_arg_iter_next(&iter);
    if (DirectoryExists(c)) {
      ChangeDirectory(c);
    } else {
      console_printlnf("Error: %s: No such directory", c);
    }
  }
}
