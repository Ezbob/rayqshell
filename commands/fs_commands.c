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
    struct console_arg arg;

    for (int i = 0; console_arg_iter_next_arg(&iter, &arg) != -1; ++i) {
      char const *c = console_arg_as_str(&arg);
      if (i > 0) {
        console_printlnf("===> '%s'", c);
      }
      ls_print_file(c);
    }
  }
}
