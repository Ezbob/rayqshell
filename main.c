
#include "raylib.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "console.h"
#include "console_args.h"

void c_print(int cs, int *cc) {
  struct console_arg_iter iter = console_arg_iter_init(cc, cs);
  struct console_arg arg;

  for (int i = 0; console_arg_iter_next_arg(&iter, &arg) != -1; ++i) {
    TraceLog(LOG_INFO, "--> ARG %i", i);
    for (int j = 0; j < arg.size; ++j) {
      TraceLog(LOG_INFO, "-> %i: %i %c", j, arg.start[j], (char) arg.start[j]);
    }
  }

}


int main(int argc, char **argv) {

  InitWindow(800, 600, "HELLO");

  console_init();

  Font f = LoadFontEx("resources/DotGothic16-Regular.ttf", 16, NULL, 1024);

  console_set_font(f, 16);

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