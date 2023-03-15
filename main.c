
#include "console.h"
#include "console_fs_commands.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {

  InitWindow(800, 600, "HELLO");

  console_init();

  Font f = LoadFontEx("resources/DotGothic16-Regular.ttf", 18, NULL, 1024);

  console_set_font(f, 18);

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