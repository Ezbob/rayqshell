
#ifndef _HEADER_FILE_console_20230115155057_
#define _HEADER_FILE_console_20230115155057_

#include "raylib.h"

void console_init();

void console_update();

void console_render();

void console_writeln(char const *blah);

void console_fwriteln(char const *format, ...);

void console_register(const char *name, void (*decision_f)(int , int*));

void console_set_active_key(int key);

bool console_is_active();

void console_set_font(Font f, float size);

void console_set_font_size(float font_size);

void console_set_background_color(Color c);

Color console_get_background_color();

void console_set_font_color(Color c);

Color console_get_font_color();

#endif