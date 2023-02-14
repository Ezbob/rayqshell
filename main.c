
#include "raylib.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "console.h"

void c_print(int cs,  int *cc) {
  console_fwriteln("I saw this count %i", console_count_args(cc, cs));
}


int main(int argc, char **argv) {

  InitWindow(800, 600, "HELLO");

  console_init();

  Font f = LoadFontEx("resources/DotGothic16-Regular.ttf", 18, NULL, 1024);

  console_set_font(f, 18);

  console_register("echo", c_print);

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