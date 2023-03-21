
#include "commands/fs_commands.h"
#include "rqshell.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char **argv) {

  InitWindow(800, 600, "HELLO");

  rqshell_init();

  Font f = LoadFontEx("resources/DotGothic16-Regular.ttf", 18, NULL, 1024);

  rqshell_set_font(f, 18);

  rqshell_register("pwd", rqshell_command_pwd);
  rqshell_register("ls", rqshell_command_ls);
  rqshell_register("cd", rqshell_command_cd);

  while (!WindowShouldClose()) {
    rqshell_update();

    BeginDrawing();
    ClearBackground(WHITE);

    rqshell_render();

    EndDrawing();
  }

  UnloadFont(f);
  return 0;
}