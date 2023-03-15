#include "console.h"
#include <raylib.h>
#include <raymath.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "console_config.h"

static inline bool is_white_space(char c) {
  return ('\n') == c || (' ') == c || ('\t') == c || ('\f') == c ||
         ('\v') == c || ('\r') == c;
}

static inline bool is_quote(char c) { return (c == '\"' || c == '\''); }

extern void console_command_clear(int len, char const *c);

extern void console_command_exit(int len, char const *c);

static void console_raylib_logging(int logLevel, const char *text,
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
    void (*value[N_DECISIONS])(int, char *);
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
    int direction;
    float move_timer;
    float blink_timer;
    float timeout;
    bool on;
  } cursor;

  char show_buffer[LINE_SIZE];
} g_console;

static inline void console_del_char(struct console *c) {
  if (c->cursor.index <= 0) return;
  int i = c->cursor.index - 1;
  for (; c->text[0][i] != '\0'; ++i) {
    c->text[0][i] = c->text[0][i + 1];
  }
  c->text[0][i + 1] = '\0';
}

static inline void console_put_char(struct console *c, char cha) {
  char *prompt_line = &c->text[0];
  char ch = prompt_line[c->cursor.index];
  for (int i = c->cursor.index + 1; ch != '\0' && i < LINE_SIZE; ++i) {
    char tmp = prompt_line[i];
    prompt_line[i] = ch;
    ch = tmp;
  }
  prompt_line[c->cursor.index] = cha;
  g_console.cursor.index += 1;
}

static inline void console_shift_up(char bufs[N_LINES][LINE_SIZE], int nbufs) {
  char n[LINE_SIZE], b[LINE_SIZE];

  strcpy(n, *bufs);
  for (int i = 1; i < (nbufs - 1); ++i) {
    strcpy(b, bufs[i]);
    strcpy(bufs[i], n);
    strcpy(n, b);
  }
}

void console_init() {
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

  g_console.view_port = (Camera2D){
      .offset = (Vector2){.x = 0, .y = 0},
      .rotation = 0.f,
      .zoom = 1.f,
  };

  console_register("exit", console_command_exit);
  console_register("clear", console_command_clear);

  //SetTraceLogCallback(console_raylib_logging);
}

static void console_raylib_logging(int logLevel, const char *text,
                                   va_list args) {
  if (logLevel >= LOG_WARNING) {
    int written = vsnprintf(g_console.text[0], LINE_SIZE, text, args);
    if (written < 0) {
      console_println("Fatal error: failed to write to console");
      return;
    }
    console_shift_up(g_console.text, N_LINES);
  }
}

void console_println(char const *blah) {
  strcpy(g_console.text[0], blah);

  console_shift_up(g_console.text, N_LINES);
}

void console_printlnf(char const *format, ...) {
  va_list args;
  va_start(args, format);

  int written = vsnprintf(g_console.text[0], LINE_SIZE, format, args);
  if (written < 0) {
    console_println("Fatal error: failed to write to console");
    return;
  }

  va_end(args);

  console_shift_up(g_console.text, N_LINES);
}

void console_register(const char *name, void (*f)(int, char const *)) {
  int j = g_console.decisions.used;
  g_console.decisions.key[j] = name;
  g_console.decisions.value[j] = f;
  g_console.decisions.used++;
}

int console_parse_prefix(char *buffer, int buffer_length) {
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
  if (strncpy(buffer, prompt_line + prefix_start, prefix_len) == NULL) {
    return -1;
  }
  buffer[buffer_length - 1] = '\0';

  return prefix_end;
}

// scan command line and find out which command to run
void console_scan() {
  char *prompt_line = g_console.history.buffer[0];
  int len = (int)strlen(prompt_line);
  if (len == 0) {
    return; // empty input
  }

  int prefix_end =
      console_parse_prefix(g_console.decisions.prefix_buffer, LINE_SIZE);
  if (prefix_end == -1) {
    console_printlnf("Error: No such command");
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

  console_printlnf("Error: %s: No such command",
                   g_console.decisions.prefix_buffer);
}

static inline void console_update_animation() {
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

static inline void console_handle_backspace() {
  if (IsKeyPressed(KEY_BACKSPACE)) {
    g_console.backspace.down = true;
    console_del_char(&g_console);
    g_console.cursor.index -= (g_console.cursor.index > 0 ? 1 : 0);
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

      console_del_char(&g_console);

      g_console.backspace.timer = 0.f;
      g_console.backspace.timeout = BACKSPACE_DELETE;
      g_console.cursor.index -= (g_console.cursor.index > 0 ? 1 : 0);
    } else if (prompt_len == 0) {
      g_console.backspace.down = false;
    }
  }
}

static inline void console_handle_cursor_move() {
  enum Cursor_Movement { NO_MOVE = 0, LEFT_MOVE, RIGHT_MOVE };
  int prompt_len = (int)strlen(g_console.text[0]);

  if (IsKeyPressed(KEY_LEFT)) {
    g_console.cursor.move_timer = 0.f;
    g_console.cursor.direction = LEFT_MOVE;
    g_console.cursor.index -= (g_console.cursor.index > 0) ? 1 : 0;
    g_console.cursor.timeout = CURSOR_MOVE_FIRST;
  } else if (IsKeyPressed(KEY_RIGHT)) {
    g_console.cursor.move_timer = 0.f;
    g_console.cursor.direction = RIGHT_MOVE;
    g_console.cursor.index += (g_console.cursor.index < prompt_len) ? 1 : 0;
    g_console.cursor.timeout = CURSOR_MOVE_FIRST;
  }

  if (IsKeyReleased(KEY_LEFT) || IsKeyReleased(KEY_RIGHT)) {
    g_console.cursor.direction = NO_MOVE;
    g_console.cursor.move_timer = 0.f;
  }

  switch (g_console.cursor.direction) {
  case LEFT_MOVE:
    g_console.cursor.move_timer += GetFrameTime();
    if (g_console.cursor.move_timer > g_console.cursor.timeout) {
      g_console.cursor.index -= (g_console.cursor.index > 0) ? 1 : 0;
      g_console.cursor.move_timer = 0.f;
      g_console.cursor.timeout = CURSOR_MOVE;
    }
    break;
  case RIGHT_MOVE:
    g_console.cursor.move_timer += GetFrameTime();
    if (g_console.cursor.move_timer > g_console.cursor.timeout) {
      g_console.cursor.index += (g_console.cursor.index < prompt_len) ? 1 : 0;
      g_console.cursor.move_timer = 0.f;
      g_console.cursor.timeout = CURSOR_MOVE;
    }
    break;
  default:
    break;
  }
}

static inline console_handle_enter() {
  if (IsKeyPressed(KEY_ENTER)) {
    if (strcmp(g_console.history.buffer[0], g_console.text[0]) != 0) {
      strcpy(g_console.history.buffer[0], g_console.text[0]);
      console_shift_up(g_console.history.buffer, N_LINES);
      g_console.history.used = g_console.history.used < N_LINES
                                   ? g_console.history.used + 1
                                   : (N_LINES - 1);
    }

    console_shift_up(g_console.text, N_LINES);
    console_scan();
    memset(g_console.text[0], '\0', LINE_SIZE);

    g_console.history.index = g_console.cursor.index = 0;
  }
}

static inline console_handle_history() {
  if (IsKeyPressed(KEY_UP)) {
    g_console.history.index = g_console.history.index < g_console.history.used
                                  ? g_console.history.index + 1
                                  : g_console.history.used;
    strcpy(g_console.text[0],
           g_console.history.buffer[g_console.history.index]);
    g_console.cursor.index = (int)strlen(g_console.text[0]);
  } else if (IsKeyPressed(KEY_DOWN)) {
    g_console.history.index =
        g_console.history.index > 0 ? g_console.history.index - 1 : 0;
    strcpy(g_console.text[0],
           g_console.history.buffer[g_console.history.index]);
    g_console.cursor.index = (int)strlen(g_console.text[0]);
  }
}

void console_update() {
  console_update_animation();

  if (g_console.opening_animation.state != CONSOLE_OPENED) {
    return;
  }

  console_handle_history();

  console_handle_cursor_move();

  console_handle_enter();

  console_handle_backspace();

  int c = GetCharPressed();
  if (c != 0) {
    console_put_char(&g_console, c);
  }

  if (g_console.cursor.direction == 0) {
    g_console.cursor.blink_timer += GetFrameTime();
    if (g_console.cursor.blink_timer >= 0.5f) {
      g_console.cursor.on = !g_console.cursor.on;
      g_console.cursor.blink_timer = 0.f;
    }
  }

  strncpy(g_console.show_buffer, g_console.text[0], LINE_SIZE);

  if (g_console.cursor.on || g_console.cursor.direction != 0) {
    g_console.show_buffer[g_console.cursor.index] = '_';
  }

  g_console.view_port.offset.y =
      Clamp((g_console.view_port.offset.y +
             ((float)GetMouseWheelMove() * g_console.font_size)),
            0.f, N_LINES * (g_console.font_size + 2.f));
}

void console_render() {
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

void console_set_active_key(int key) { g_console.activation_key = key; }

void console_set_font(Font f, float size) {
  g_console.font = f;
  g_console.font_size = size;
}

bool console_is_active() {
  return g_console.opening_animation.state == CONSOLE_OPENED;
}

void console_set_background_color(Color c) { g_console.background_color = c; }

Color console_get_background_color() { return g_console.background_color; }

void console_set_font_size(float font_size) { g_console.font_size = font_size; }

void console_set_font_color(Color c) { g_console.font_color = c; }

Color console_get_font_color() { return g_console.font_color; }

void console_clear() {
  for (int i = 0; i < N_LINES; ++i) {
    g_console.text[i][0] = '\0';
  }
}