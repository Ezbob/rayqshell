
#include "raylib.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "console.h"
#include "console_args.h"

void c_print(int cs, char const *cc) {
  struct console_arg_iter iter = console_arg_iter_init(cc, cs);
  struct console_arg arg;

  for (int i = 0; console_arg_iter_next_arg(&iter, &arg) != -1; ++i) {
    TraceLog(LOG_INFO, "--> ARG %i", i);
    for (int j = 0; j < arg.size; ++j) {
      TraceLog(LOG_INFO, "-> %i: %i %c", j, arg.start[j], (char) arg.start[j]);
    }
  }

}

void command_pwd(int cs, char const *cc) {
  if (cs > 0) {
    console_writeln("'ls' does not take any arguments");
    return;
  }
  console_writeln(GetWorkingDirectory());
}

void command_ls(int cs, int *cc) {
  struct console_arg_iter iter = console_arg_iter_init(cc, cs);
  if (console_arg_iter_count_args(&iter) == 0) {
    char const *pwd = GetWorkingDirectory();

    FilePathList p = LoadDirectoryFiles(pwd);

    for (unsigned i = 0; i < p.count; ++i) {
      console_writeln(p.paths[i]);
    }

    UnloadDirectoryFiles(p);
  } else {

  }
}


int main(int argc, char **argv) {

  InitWindow(800, 600, "HELLO");

  console_init();

  Font f = LoadFontEx("resources/DotGothic16-Regular.ttf", 18, NULL, 1024);

  console_set_font(f, 18);

  console_register("echo", c_print);
  console_register("pwd", command_pwd);
  console_register("ls", command_ls);

  while (!WindowShouldClose()) {

    console_update();

    BeginDrawing();
    ClearBackground(WHITE);
    console_render();
    EndDrawing();
  }

  UnloadFont(f);
  return 0;
}