
#ifndef _HEADER_FILE_console_20230115155057_
#define _HEADER_FILE_console_20230115155057_

#include "raylib.h"

/*
 * The consoles one-time initialization routine.
 * Must be called only once, before any update or 
 * render steps as been done.
 */
void console_init();

/*
 * Update step of the console.
 * Must be called in your application's update step.
 */
void console_update();

/*
 * Render step of the console.
 * Must be called in your application's render step.
 */
void console_render();

/*
 * Write a line to the console.
 */
void console_writeln(char const *blah);

/*
 * Write a formatted line to the console.
 * Wraps around C standard library printf functionality, so
 * the same format rules apply here.
 */
void console_fwriteln(char const *format, ...);

/*
 * Register an function handler that gets called when
 * the given prefix is observed from the user input.
 * 
 * This is how extend the functionality of the console.
 */
void console_register(const char *prefix, void (*handler)(int , char const*));

/*
 * Set the console's activation key.
 * The key code recognized is the same as used by raylib keyboard input codes.
 * 
 * The default activation key is KEY_F3.
 */
void console_set_active_key(int key);

/*
 * Query whether the console is active and ready for user input.
 *
 * Returns true iff the console is active, false otherwise.
 */
bool console_is_active();

/*
 * Set the font used in the console.
 *
 * The second argument is the font size.
 */
void console_set_font(Font f, float size);

/*
 * Change the font size used in the console.
 */
void console_set_font_size(float font_size);

/*
 * Change the background color.
 */
void console_set_background_color(Color c);

/*
 * Get the current background color.
 */
Color console_get_background_color();

/*
 * Change the font color.
 */
void console_set_font_color(Color c);

/*
 * Get the current font color.
 */
Color console_get_font_color();

#endif