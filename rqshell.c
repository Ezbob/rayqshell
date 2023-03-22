#include "rqshell.h"
#include "rqshell_config.h"
#include <raylib.h>
#include <raymath.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline bool is_white_space(char c) {
  return (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f');
}

extern void rqshell_command_clear(int len, char const *c);

extern void rqshell_command_exit(int len, char const *c);

static void rqshell_raylib_logging(int logLevel, const char *text,
                                   va_list args);

struct console {
  char text[N_LINES][LINE_SIZE];
  Rectangle window;
  Camera2D view_port;

  int activation_key;
  Font font;
  float font_size;

  Color background_color;
  Color font_color;

  struct {
    char prefix_buffer[LINE_SIZE];
    const char *key[N_DECISIONS];
    void (*value[N_DECISIONS])(int, char const *);
    int used;
  } decisions;

  struct {
    bool down;
    float timer;
    float timeout;
  } backspace;

  struct {
    char buffer[N_LINES][LINE_SIZE];
    unsigned index;
    unsigned used;
  } history;

  struct {
    float percent;
    float timer;
    enum {
      CONSOLE_CLOSED,
      CONSOLE_OPENED,
      CONSOLE_OPENING,
      CONSOLE_CLOSING,
    } state;
  } opening_animation;

  struct {
    int index;
    int byteoffset;
    enum Cursor_Movement {
      CURSOR_NO_MOVE = 0,
      CURSOR_LEFT_MOVE,
      CURSOR_RIGHT_MOVE
    } direction;
    float move_timer;
    float blink_timer;
    float timeout;
    bool on;
  } cursor;

  char show_buffer[LINE_SIZE];
} g_console;

static inline void rqshell_move_left(int byte_size) {
  if (g_console.cursor.byteoffset <= 0) {
    return;
  }
  char *prompt_line = &(g_console.text[0][0]);

  if (byte_size == 0) {
    GetCodepointPrevious(prompt_line + g_console.cursor.byteoffset, &byte_size);
  }

  g_console.cursor.byteoffset -= byte_size;
  g_console.cursor.index--;
}

static inline void rqshell_move_right(int byte_size) {
  if (g_console.cursor.byteoffset >= strlen(g_console.text[0])) {
    return;
  }
  char *prompt_line = &(g_console.text[0][0]);

  if (byte_size == 0) {
    GetCodepointNext(prompt_line + g_console.cursor.byteoffset, &byte_size);
  }

  g_console.cursor.byteoffset += byte_size;
  g_console.cursor.index++;
}

/*
// get byte offset into text counting "nCodepoints"-number of codepoints
static inline int CodepointGetByteOffset(const char *text, int nCodepoints) {
  int byteOffset = 0;
  if (nCodepoints * 4 >= LINE_SIZE) {
    return -1;
  }

  for (int codepointCount = 0;
       byteOffset < LINE_SIZE && codepointCount < nCodepoints;
       codepointCount++) {
    int codepointSize = 0;
    GetCodepointNext(text + byteOffset, &codepointSize);
    byteOffset += codepointSize;
  }
  return byteOffset;
}
*/

static inline void rqshell_del_char(struct console *c) {
  if (c->cursor.index <= 0) {
    return;
  }
  char *prompt_line = &(c->text[0][0]);

  char *deletion_point = prompt_line + g_console.cursor.byteoffset;
  int rest_size = strlen(deletion_point) + 1;

  int utfsize = 0;
  GetCodepointPrevious(deletion_point, &utfsize);

  memmove(deletion_point - utfsize, deletion_point, rest_size);

  rqshell_move_left(utfsize);
}

static inline void rqshell_put_char(struct console *c, int cha) {
  if (c->cursor.index >= LINE_SIZE) {
    return;
  }
  char *prompt_line = &(c->text[0][0]);

  char *insertion_point = prompt_line + g_console.cursor.byteoffset;
  int rest_size = strlen(insertion_point) + 1;

  int utfsize = 0;
  const char *point = CodepointToUTF8(cha, &utfsize);

  memmove(insertion_point + utfsize, insertion_point, rest_size);
  memcpy(insertion_point, point, utfsize);

  rqshell_move_right(utfsize);
}

static inline void rqshell_shift_up(char bufs[N_LINES][LINE_SIZE], int nbufs) {
  char n[LINE_SIZE], b[LINE_SIZE];

  memcpy(n, *bufs, sizeof(n));
  for (int i = 1; i < (nbufs - 1); ++i) {
    memcpy(b, bufs[i], sizeof(b));
    memcpy(bufs[i], n, sizeof(bufs[i]));
    memcpy(n, b, sizeof(n));
  }
}

void rqshell_init() {
  for (int i = 0; i < N_LINES; ++i) {
    memset(g_console.text + i, '\0', LINE_SIZE);
  }

  g_console.window = (Rectangle){
      .width = (float)GetScreenWidth(),
      .x = 0.f,
      .y = 0.f,
  };

  g_console.decisions.used = 0;
  for (int i = 0; i < N_DECISIONS; ++i) {
    g_console.decisions.value[i] = NULL;
    g_console.decisions.key[i] = NULL;
  }

  g_console.font_size = 14.0f;
  g_console.activation_key = KEY_F3;
  g_console.font = GetFontDefault();
  g_console.backspace.down = false;
  g_console.backspace.timer = 0.f;
  g_console.backspace.timeout = 0.5f;
  g_console.history.index = g_console.history.used = 0;
  g_console.background_color = (Color){.r = 0, .b = 0, .g = 0, .a = 210};
  g_console.font_color = (Color){.r = 0, .b = 0, .g = 255, .a = 255};

  g_console.opening_animation.timer = 0.f;
  g_console.opening_animation.percent = 0.f;
  g_console.opening_animation.state = CONSOLE_CLOSED;

  g_console.cursor.index = 0;
  g_console.cursor.on = true;
  g_console.cursor.blink_timer = 0.f;
  g_console.cursor.move_timer = 0.f;
  g_console.cursor.direction = 0;
  g_console.cursor.byteoffset = 0;

  g_console.view_port = (Camera2D){
      .offset = (Vector2){.x = 0, .y = 0},
      .rotation = 0.f,
      .zoom = 1.f,
  };

  rqshell_register("exit", rqshell_command_exit);
  rqshell_register("clear", rqshell_command_clear);
}

void rqshell_println(char const *blah) {
  memcpy(g_console.text[0], blah, sizeof(g_console.text[0]));
  rqshell_shift_up(g_console.text, N_LINES);
}

void rqshell_printlnf(char const *format, ...) {
  va_list args;
  va_start(args, format);

  int written = vsnprintf(g_console.text[0], LINE_SIZE, format, args);
  if (written < 0) {
    rqshell_println("Fatal error: failed to write to console");
    return;
  }

  va_end(args);

  rqshell_shift_up(g_console.text, N_LINES);
}

void rqshell_register(const char *name, void (*f)(int, char const *)) {
  int j = g_console.decisions.used;
  g_console.decisions.key[j] = name;
  g_console.decisions.value[j] = f;
  g_console.decisions.used++;
}

int rqshell_parse_prefix(char *buffer, int buffer_length) {
  char *prompt_line = g_console.text[0];
  int len = (int)strlen(prompt_line);
  if (len == 0) {
    return -1; // empty input
  }

  int prefix_end = 0, prefix_start = 0, prefix_len = 0;

  for (; prefix_start < len; ++prefix_start) {
    if (!is_white_space(prompt_line[prefix_start])) {
      break;
    }
  }

  for (prefix_end = prefix_start; prefix_end < len; ++prefix_end) {
    if (is_white_space(prompt_line[prefix_end])) {
      break;
    }
  }

  prefix_len = (prefix_end - prefix_start);

  memset(buffer, '\0', LINE_SIZE);
  if (memcpy(buffer, prompt_line + prefix_start, prefix_len) == NULL) {
    return -1;
  }
  buffer[buffer_length - 1] = '\0';

  return prefix_end;
}

// scan command line and find out which command to run
void rqshell_scan() {
  char *prompt_line = g_console.history.buffer[0];
  int len = (int)strlen(prompt_line);
  if (len == 0) {
    return; // empty input
  }

  int prefix_end =
      rqshell_parse_prefix(g_console.decisions.prefix_buffer, LINE_SIZE);
  if (prefix_end == -1) {
    rqshell_printlnf("Error: No such command");
    return;
  }

  int prefix_len = (int)strlen(g_console.decisions.prefix_buffer);

  for (int decision_index = 0; decision_index < g_console.decisions.used;
       ++decision_index) {

    const char *key = g_console.decisions.key[decision_index];
    if (strlen(key) < prefix_len) {
      continue;
    }

    if (strcmp(g_console.decisions.prefix_buffer, key) == 0) {
      int start_of_args = prefix_end;
      for (; start_of_args < len && is_white_space(prompt_line[start_of_args]);
           start_of_args++)
        ;

      (*g_console.decisions.value[decision_index])((len - start_of_args),
                                                   prompt_line + start_of_args);
      return;
    }
  }

  rqshell_printlnf("Error: %s: No such command",
                   g_console.decisions.prefix_buffer);
}

static inline void rqshell_update_animation() {
  if (IsKeyPressed(g_console.activation_key)) {
    if (g_console.opening_animation.state == CONSOLE_CLOSED) {
      g_console.opening_animation.state = CONSOLE_OPENING;
    } else if (g_console.opening_animation.state == CONSOLE_OPENED) {
      g_console.opening_animation.state = CONSOLE_CLOSING;
    }
  }

  if (g_console.opening_animation.state == CONSOLE_CLOSING ||
      g_console.opening_animation.state == CONSOLE_OPENING) {
    g_console.opening_animation.timer += GetFrameTime();
    g_console.window.height =
        Lerp(0.f, GetScreenHeight() / 3.f, g_console.opening_animation.percent);
  }

  if (g_console.opening_animation.state == CONSOLE_OPENING &&
      g_console.opening_animation.timer > 0.001f) {
    g_console.opening_animation.percent =
        Clamp(g_console.opening_animation.percent + 0.01f, 0.f, 1.f);
    g_console.opening_animation.timer = 0.f;
    if (g_console.opening_animation.percent >= 1.f) {
      g_console.opening_animation.state = CONSOLE_OPENED;
    }
  } else if (g_console.opening_animation.state == CONSOLE_CLOSING &&
             g_console.opening_animation.timer > 0.001f) {
    g_console.opening_animation.percent =
        Clamp(g_console.opening_animation.percent - 0.01f, 0.f, 1.f);
    g_console.opening_animation.timer = 0.f;
    if (g_console.opening_animation.percent <= 0.f) {
      g_console.opening_animation.state = CONSOLE_CLOSED;
    }
  }
}

static inline void rqshell_handle_backspace() {
  if (IsKeyPressed(KEY_BACKSPACE)) {
    g_console.backspace.down = true;
    rqshell_del_char(&g_console);
  }

  if (IsKeyReleased(KEY_BACKSPACE)) {
    g_console.backspace.down = false;
    g_console.backspace.timer = 0.f;
    g_console.backspace.timeout = BACKSPACE_DELETE_FIRST;
  }

  if (g_console.backspace.down) {
    g_console.backspace.timer += GetFrameTime();
    int prompt_len = (int)strlen(g_console.text[0]);
    if (prompt_len > 0 &&
        g_console.backspace.timer > g_console.backspace.timeout) {

      rqshell_del_char(&g_console);

      g_console.backspace.timer = 0.f;
      g_console.backspace.timeout = BACKSPACE_DELETE;
    } else if (prompt_len == 0) {
      g_console.backspace.down = false;
    }
  }
}

static inline void rqshell_handle_cursor_move() {

  int prompt_len = (int)strlen(g_console.text[0]);

  if (IsKeyPressed(KEY_LEFT)) {
    g_console.cursor.move_timer = 0.f;
    g_console.cursor.direction = CURSOR_LEFT_MOVE;
    g_console.cursor.timeout = CURSOR_MOVE_FIRST;
    rqshell_move_left(0);

  } else if (IsKeyPressed(KEY_RIGHT)) {
    g_console.cursor.move_timer = 0.f;
    g_console.cursor.direction = CURSOR_RIGHT_MOVE;
    g_console.cursor.timeout = CURSOR_MOVE_FIRST;
    rqshell_move_right(0);
  }

  if (IsKeyReleased(KEY_LEFT) || IsKeyReleased(KEY_RIGHT)) {
    g_console.cursor.direction = CURSOR_NO_MOVE;
    g_console.cursor.move_timer = 0.f;
  }

  switch (g_console.cursor.direction) {
  case CURSOR_LEFT_MOVE:
    g_console.cursor.move_timer += GetFrameTime();
    if (g_console.cursor.move_timer > g_console.cursor.timeout) {
      rqshell_move_left(0);
      g_console.cursor.move_timer = 0.f;
      g_console.cursor.timeout = CURSOR_MOVE;
    }
    break;
  case CURSOR_RIGHT_MOVE:
    g_console.cursor.move_timer += GetFrameTime();
    if (g_console.cursor.move_timer > g_console.cursor.timeout) {
      rqshell_move_right(0);
      g_console.cursor.move_timer = 0.f;
      g_console.cursor.timeout = CURSOR_MOVE;
    }
    break;
  default:
    break;
  }
}

static inline void rqshell_handle_enter() {
  if (IsKeyPressed(KEY_ENTER)) {
    if (strcmp(g_console.history.buffer[0], g_console.text[0]) != 0) {
      memcpy(g_console.history.buffer[0], g_console.text[0],
             sizeof(g_console.history.buffer[0]));
      rqshell_shift_up(g_console.history.buffer, N_LINES);
      g_console.history.used = g_console.history.used < N_LINES
                                   ? g_console.history.used + 1
                                   : (N_LINES - 1);
    }

    rqshell_shift_up(g_console.text, N_LINES);
    rqshell_scan();
    memset(g_console.text[0], '\0', LINE_SIZE);

    g_console.history.index = g_console.cursor.index = 0;
  }
}

static inline void rqshell_handle_history() {
  if (IsKeyPressed(KEY_UP)) {
    g_console.history.index = g_console.history.index < g_console.history.used
                                  ? g_console.history.index + 1
                                  : g_console.history.used;
    memcpy(g_console.text[0], g_console.history.buffer[g_console.history.index],
           sizeof(g_console.text[0]));
    g_console.cursor.index = (int)strlen(g_console.text[0]);
  } else if (IsKeyPressed(KEY_DOWN)) {
    g_console.history.index =
        g_console.history.index > 0 ? g_console.history.index - 1 : 0;
    memcpy(g_console.text[0], g_console.history.buffer[g_console.history.index],
           sizeof(g_console.text[0]));
    g_console.cursor.index = (int)strlen(g_console.text[0]);
  }
}

void rqshell_update() {
  rqshell_update_animation();

  if (g_console.opening_animation.state != CONSOLE_OPENED) {
    return;
  }

  rqshell_handle_history();

  rqshell_handle_cursor_move();

  rqshell_handle_enter();

  rqshell_handle_backspace();

  int c = GetCharPressed();
  if (c != 0) {
    rqshell_put_char(&g_console, c);
  }

  if (g_console.cursor.direction == CURSOR_NO_MOVE) {
    g_console.cursor.blink_timer += GetFrameTime();
    if (g_console.cursor.blink_timer >= 0.5f) {
      g_console.cursor.on = !g_console.cursor.on;
      g_console.cursor.blink_timer = 0.f;
    }
  }

  memcpy(g_console.show_buffer, g_console.text[0], LINE_SIZE);

  if (g_console.cursor.on || g_console.cursor.direction != CURSOR_NO_MOVE) {
    int utf8_size = 0;
    GetCodepointNext(g_console.show_buffer + g_console.cursor.byteoffset,
                     &utf8_size);

    if (utf8_size > 1) {
      memmove(
          g_console.show_buffer + g_console.cursor.byteoffset + (utf8_size - 1),
          g_console.show_buffer + g_console.cursor.byteoffset + utf8_size,
          strlen(g_console.show_buffer + g_console.cursor.byteoffset + utf8_size) +
              1);
    }

    g_console.show_buffer[g_console.cursor.byteoffset] = '_';
  }

  g_console.view_port.offset.y =
      Clamp((g_console.view_port.offset.y +
             ((float)GetMouseWheelMove() * g_console.font_size)),
            0.f, N_LINES * (g_console.font_size + 2.f));
}

void rqshell_render() {
  DrawRectangleRec(g_console.window, g_console.background_color);
  BeginScissorMode((int)g_console.window.x, (int)g_console.window.y,
                   (int)g_console.window.width, (int)g_console.window.height);
  BeginMode2D(g_console.view_port);

  float prompt_height = (g_console.window.y + g_console.window.height) -
                        (g_console.font_size + 2.f);
  DrawTextEx(g_console.font, g_console.show_buffer,
             (Vector2){.x = 0, .y = prompt_height}, g_console.font_size, 1.2f,
             g_console.font_color);

  for (int i = 1; i < N_LINES; ++i) {
    float hn = (g_console.window.y + g_console.window.height) -
               ((g_console.font_size + 2.f) * (i + 1));

    DrawTextEx(g_console.font, g_console.text[i], (Vector2){.x = 0, .y = hn},
               g_console.font_size, 1.2f, g_console.font_color);
  }
  EndMode2D();
  EndScissorMode();
}

void rqshell_set_active_key(int key) { g_console.activation_key = key; }

void rqshell_set_font(Font f, float size) {
  g_console.font = f;
  g_console.font_size = size;
}

bool rqshell_is_active() {
  return g_console.opening_animation.state == CONSOLE_OPENED;
}

void rqshell_set_background_color(Color c) { g_console.background_color = c; }

Color rqshell_get_background_color() { return g_console.background_color; }

void rqshell_set_font_size(float font_size) { g_console.font_size = font_size; }

void rqshell_set_font_color(Color c) { g_console.font_color = c; }

Color rqshell_get_font_color() { return g_console.font_color; }

void rqshell_clear() {
  for (int i = 0; i < N_LINES; ++i) {
    g_console.text[i][0] = '\0';
  }
  g_console.cursor.index = 0;
  g_console.cursor.direction = CURSOR_NO_MOVE;
}