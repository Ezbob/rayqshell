#include "console.h"
#include <raylib.h>
#include <raymath.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N_LINES 255
#define LINE_SIZE 1024
#define N_DECISIONS 255

#define BACKSPACE_DELETE_FIRST 0.5f
#define BACKSPACE_DELETE .03f

#define CURSOR_MOVE_FIRST 0.5f
#define CURSOR_MOVE 0.03f

static inline bool is_white_space(char c) {
  return ('\n') == c || (' ') == c || ('\t') == c || ('\f') == c ||
         ('\v') == c || ('\r') == c;
}

static inline bool is_quote(char c) { return (c == '\"' || c == '\''); }

void console_builtin_clear(int len, char const *c);
void console_builtin_exit(int len, char const *c);

struct buf {
  char chs[LINE_SIZE];
  int used;
};

void buf_cpy(struct buf *dst, struct buf *src) {
  dst->used = src->used;
  memcpy(dst->chs, src->chs, LINE_SIZE);
}

void buf_put_char(struct buf *buf, char c) {
  if (buf->used < LINE_SIZE) {
    buf->chs[buf->used++] = c;
  }
}

void buf_put_char_at(struct buf *buf, int index, char c) {
  buf->used += 1;
  char ch = buf->chs[index];
  for (int i = index + 1; i < buf->used; ++i) {
    char tmp = buf->chs[i];
    buf->chs[i] = ch;
    ch = tmp;
  }
  buf->chs[index] = c;
}

void buf_del_char_at(struct buf *buf, int index) {
  buf->used -= 1;
  for (int i = index; i < buf->used; ++i) {
    buf->chs[i] = buf->chs[i + 1];
  }
  buf->chs[buf->used] = '\0';
}

void buf_reset(struct buf *buf) {
  buf->used = 0;
  memset(buf->chs, '\0', LINE_SIZE);
}

bool buf_equal(struct buf *a, struct buf *b) {
  if (a->used != b->used) {
    return false;
  }
  for (int i = 0; i < a->used; ++i) {
    if (a->chs[i] != b->chs[i]) {
      return false;
    }
  }
  return true;
}

void buf_array_shift_up(struct buf *array, int size) {
  struct buf n, b;
  buf_cpy(&n, array);
  for (int i = 1; i < (size - 1); ++i) {
    buf_cpy(&b, array + i);
    buf_cpy(array + i, &n);
    buf_cpy(&n, &b);
  }
}

struct console {
  struct buf text[N_LINES];
  Rectangle window;

  int key;

  Font font;
  float font_size;

  Color background_color;
  Color font_color;

  struct {
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
    struct buf buffer[N_LINES];
    unsigned index;
    unsigned used;
  } history;

  struct {
    float percent;
    float timer;
    enum console_enable_animation {
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

  struct buf show;
} g_console;

void console_init() {
  for (int i = 0; i < N_LINES; ++i) {
    g_console.text[i].used = 0;
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
  g_console.key = KEY_F3;
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

  console_register("exit", console_builtin_exit);
  console_register("clear", console_builtin_clear);
}

void console_writeln(char const *blah) {
  struct buf *b = g_console.text;
  b->used = (int)strlen(blah);
  memcpy(b->chs, blah, b->used);
  buf_array_shift_up(g_console.text, N_LINES);
}

void console_fwriteln(char const *format, ...) {
  va_list args;
  va_start(args, format);

  char buffer[LINE_SIZE];

  int written = vsnprintf(buffer, LINE_SIZE, format, args);
  if (written < 0) {
    console_writeln("Fatal error: failed to write to console");
    return;
  }

  struct buf *b = g_console.text;
  b->used = written;
  memcpy(b->chs, buffer, b->used);

  va_end(args);

  buf_array_shift_up(g_console.text, N_LINES);
}

void console_register(const char *name, void (*f)(int, char const *)) {
  int j = g_console.decisions.used;
  g_console.decisions.key[j] = name;
  g_console.decisions.value[j] = f;
  g_console.decisions.used++;
}

int console_parse_prefix(char *buffer, int buffer_length) {
  struct buf *prompt_line = g_console.text;
  if (prompt_line->used == 0) {
    return -1; // empty input
  }

  memset(buffer, '\0', buffer_length);

  int prefix_end = 0, prefix_start = 0, prefix_len = 0;

  for (; prefix_start < prompt_line->used; ++prefix_start) {
    if (!is_white_space(prompt_line->chs[prefix_start])) {
      break;
    }
  }

  for (prefix_end = prefix_start; prefix_end < prompt_line->used;
       ++prefix_end) {
    if (is_white_space(prompt_line->chs[prefix_end])) {
      break;
    }
  }

  prefix_len = (prefix_end - prefix_start);

  if (prefix_len > (buffer_length - 1)) {
    return -1;
  }

  if (memcpy(buffer, prompt_line->chs + prefix_start, prefix_len) == NULL) {
    return -1;
  }
  buffer[buffer_length - 1] = '\0';

  return prefix_end;
}

// scan command line and find out which command to run
void console_scan() {
  static char prefix[LINE_SIZE];

  struct buf *prompt_line = g_console.text;
  if (prompt_line->used == 0) {
    return; // empty input
  }

  int prefix_end = console_parse_prefix(prefix, LINE_SIZE);
  if (prefix_end == -1) {
    console_fwriteln("Error: No such command");
    return;
  }

  int prefix_len = ((int)strlen(prefix));

  for (int decision_index = 0; decision_index < g_console.decisions.used;
       ++decision_index) {

    const char *key = g_console.decisions.key[decision_index];
    if (strlen(key) < prefix_len) {
      continue;
    }

    if (strcmp(prefix, key) == 0) {
      int start_of_args = prefix_end;
      for (; start_of_args < prompt_line->used &&
             is_white_space(prompt_line->chs[start_of_args]);
           start_of_args++)
        ;

      (*g_console.decisions.value[decision_index])(
          (prompt_line->used - start_of_args),
          prompt_line->chs + start_of_args);
      return;
    }
  }

  console_fwriteln("Error: %s: No such command", prefix);
}

static inline void console_update_animation() {
  if (IsKeyPressed(g_console.key)) {
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
    g_console.opening_animation.percent = Clamp(g_console.opening_animation.percent + 0.01f, 0.f, 1.f);
    g_console.opening_animation.timer = 0.f;
    if (g_console.opening_animation.percent >= 1.f) {
      g_console.opening_animation.state = CONSOLE_OPENED;
    }
  } else if (g_console.opening_animation.state == CONSOLE_CLOSING &&
             g_console.opening_animation.timer > 0.001f) {
    g_console.opening_animation.percent = Clamp(g_console.opening_animation.percent - 0.01f, 0.f, 1.f);
    g_console.opening_animation.timer = 0.f;
    if (g_console.opening_animation.percent <= 0.f) {
      g_console.opening_animation.state = CONSOLE_CLOSED;
    }
  }
}

static inline void console_handle_backspace() {
  if (IsKeyPressed(KEY_BACKSPACE)) {
    g_console.backspace.down = true;
    buf_del_char_at(g_console.text, g_console.cursor.index);
    g_console.cursor.index--;
  }

  if (IsKeyReleased(KEY_BACKSPACE)) {
    g_console.backspace.down = false;
    g_console.backspace.timer = 0.f;
    g_console.backspace.timeout = BACKSPACE_DELETE_FIRST;
  }

  if (g_console.backspace.down) {
    g_console.backspace.timer += GetFrameTime();
    if (g_console.text[0].used > 0 &&
        g_console.backspace.timer > g_console.backspace.timeout) {

      buf_del_char_at(g_console.text, g_console.cursor.index);

      g_console.backspace.timer = 0.f;
      g_console.backspace.timeout = BACKSPACE_DELETE;
      g_console.cursor.index--;
    } else if (g_console.text[0].used == 0) {
      g_console.backspace.down = false;
    }
  }
}

static inline void console_handle_cursor_move() {
  enum Cursor_Movement { NO_MOVE = 0, LEFT_MOVE, RIGHT_MOVE };

  if (IsKeyPressed(KEY_LEFT)) {
    g_console.cursor.move_timer = 0.f;
    g_console.cursor.direction = LEFT_MOVE;
    g_console.cursor.index -= (g_console.cursor.index > 0) ? 1 : 0;
    g_console.cursor.timeout = CURSOR_MOVE_FIRST;
  } else if (IsKeyPressed(KEY_RIGHT)) {
    g_console.cursor.move_timer = 0.f;
    g_console.cursor.direction = RIGHT_MOVE;
    g_console.cursor.index +=
        (g_console.cursor.index < g_console.text[0].used) ? 1 : 0;
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
      g_console.cursor.index +=
          (g_console.cursor.index < g_console.text[0].used) ? 1 : 0;
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
    if (!buf_equal(g_console.history.buffer, g_console.text)) {
      buf_cpy(g_console.history.buffer, g_console.text);
      buf_array_shift_up(g_console.history.buffer, N_LINES);
      g_console.history.used =
          g_console.history.used < N_LINES ? g_console.history.used + 1 : (N_LINES - 1);
    }

    buf_array_shift_up(g_console.text, N_LINES);
    console_scan();
    buf_reset(g_console.text);

    g_console.history.index = g_console.cursor.index = 0;
  }
}

static inline console_handle_history() {
  if (IsKeyPressed(KEY_UP)) {
    g_console.history.index = g_console.history.index < g_console.history.used
                               ? g_console.history.index + 1
                               : g_console.history.used;
    buf_cpy(g_console.text, g_console.history.buffer + g_console.history.index);
    g_console.cursor.index = g_console.text->used;
  } else if (IsKeyPressed(KEY_DOWN)) {
    g_console.history.index =
        g_console.history.index > 0 ? g_console.history.index - 1 : 0;
    buf_cpy(g_console.text, g_console.history.buffer + g_console.history.index);
    g_console.cursor.index = g_console.text->used;
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
    buf_put_char_at(&g_console.text[0], g_console.cursor.index, (char)c);
    g_console.cursor.index +=
        (g_console.cursor.index < g_console.text[0].used) ? 1 : 0;
  }

  if (g_console.cursor.direction == 0) {
    g_console.cursor.blink_timer += GetFrameTime();
    if (g_console.cursor.blink_timer >= 0.5f) {
      g_console.cursor.on = !g_console.cursor.on;
      g_console.cursor.blink_timer = 0.f;
    }
  }

  buf_cpy(&g_console.show, g_console.text);

  if (g_console.cursor.on || g_console.cursor.direction != 0) {
    g_console.show.chs[g_console.cursor.index] = '_';
  }
}

void console_render() {
  DrawRectangleRec(g_console.window, g_console.background_color);

  float prompt_height = (g_console.window.y + g_console.window.height) -
                        (g_console.font_size + 2.f);
  DrawTextEx(g_console.font, g_console.show.chs,
             (Vector2){.x = 0, .y = prompt_height}, g_console.font_size, 1.2f,
             g_console.font_color);

  for (int i = 1; i < N_LINES; ++i) {
    float hn = (g_console.window.y + g_console.window.height) -
               ((g_console.font_size + 2.f) * (i + 1));

    DrawTextEx(g_console.font, g_console.text[i].chs,
               (Vector2){.x = 0, .y = hn}, g_console.font_size, 1.2f,
               g_console.font_color);
  }
}

void console_set_active_key(int key) { g_console.key = key; }

void console_set_font(Font f, float size) {
  g_console.font = f;
  g_console.font_size = size;
}

bool console_is_active() { return g_console.opening_animation.state == CONSOLE_OPENED; }

void console_set_background_color(Color c) { g_console.background_color = c; }

Color console_get_background_color() { return g_console.background_color; }

void console_set_font_size(float font_size) { g_console.font_size = font_size; }

void console_set_font_color(Color c) { g_console.font_color = c; }

Color console_get_font_color() { return g_console.font_color; }

// -------------------------------------------------------------------------------------
// builtins
// -------------------------------------------------------------------------------------

void console_builtin_exit(int len, char const *c) {
  int ec = 0;
  if (len > 1) {
    console_writeln("Error: command 'exit' does only take one argument");
    return;
  } else if (len > 0) {
    ec = atoi(c);
  }

  exit(ec);
}

void console_builtin_clear(int len, char const *c) {
  if (len > 0) {
    console_writeln("Error: command 'clear' does not take any arguments");
    return;
  }

  for (int i = 0; i < N_LINES; ++i) {
    g_console.text[i].used = 0;
  }
}

void console_builtin_help(int len, char const *c) {
  console_writeln("builtin help:");
  console_writeln("    clear               : clears the text pane of text");
  console_writeln(
      "    exit <exit_code>    : exits the program with exit code <exit_code>");
  console_writeln("");
}
