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
#define BACKSPACE_DELETE .025f

static inline bool is_white_space(char c) {
  return ('\n') == c || (' ') == c || ('\t') == c || ('\f') == c ||
         ('\v') == c || ('\r') == c;
}

static inline bool is_quote(char c) { return (c == '\"' || c == '\''); }

void console_builtin_clear(int len, char const *c);
void console_builtin_exit(int len, char const *c);

struct decisions {
  const char *key[N_DECISIONS];
  void (*value[N_DECISIONS])(int, char *);
  int used;
};

struct buf {
  char chs[LINE_SIZE];
  int used;
};

void buf_cpy(struct buf *dst, struct buf *src) {
  dst->used = src->used;
  for (int i = 0; i < LINE_SIZE; ++i) {
    dst->chs[i] = src->chs[i];
  }
}

void buf_put_char(struct buf *buf, char c) {
  if (buf->used < LINE_SIZE) {
    buf->chs[buf->used++] = c;
  }
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

struct history {
  struct buf buffer[N_LINES];
  unsigned index;
  unsigned used;
};

struct console {
  struct buf text[N_LINES];
  Rectangle window;
  struct decisions decisions;
  int key;

  Font font;
  float font_size;
  Color font_color;

  Color background_color;

  struct backspace {
    bool down;
    float timer;
    float timeout;
  } backspace;

  struct history hist;

  struct enable_animation {
    float percent;
    float timer;
    enum console_enable_animation {
      CONSOLE_CLOSED,
      CONSOLE_OPENED,
      CONSOLE_OPENING,
      CONSOLE_CLOSING,
    } state;
  } anim;

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
  g_console.hist.index = g_console.hist.used = 0;
  g_console.background_color = (Color){.r = 0, .b = 0, .g = 0, .a = 210};
  g_console.font_color = (Color){.r = 0, .b = 0, .g = 255, .a = 255};

  g_console.anim.timer = 0.f;
  g_console.anim.percent = 0.f;
  g_console.anim.state = CONSOLE_CLOSED;

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
          (prompt_line->used - start_of_args), prompt_line->chs + start_of_args);
      return;
    }
  }

  console_fwriteln("Error: %s: No such command", prefix);
}

static inline void console_update_animation() {
  if (IsKeyPressed(g_console.key)) {
    if (g_console.anim.state == CONSOLE_CLOSED) {
      g_console.anim.state = CONSOLE_OPENING;
    } else if (g_console.anim.state == CONSOLE_OPENED) {
      g_console.anim.state = CONSOLE_CLOSING;
    }
  }

  if (g_console.anim.state == CONSOLE_CLOSING ||
      g_console.anim.state == CONSOLE_OPENING) {
    g_console.anim.timer += GetFrameTime();
    g_console.window.height =
        Lerp(0.f, GetScreenHeight() / 3.f, g_console.anim.percent);
  }

  if (g_console.anim.state == CONSOLE_OPENING &&
      g_console.anim.timer > 0.001f) {
    g_console.anim.percent = Clamp(g_console.anim.percent + 0.01f, 0.f, 1.f);
    g_console.anim.timer = 0.f;
    if (g_console.anim.percent >= 1.f) {
      g_console.anim.state = CONSOLE_OPENED;
    }
  } else if (g_console.anim.state == CONSOLE_CLOSING &&
             g_console.anim.timer > 0.001f) {
    g_console.anim.percent = Clamp(g_console.anim.percent - 0.01f, 0.f, 1.f);
    g_console.anim.timer = 0.f;
    if (g_console.anim.percent <= 0.f) {
      g_console.anim.state = CONSOLE_CLOSED;
    }
  }
}

void console_update() {
  console_update_animation();

  if (g_console.anim.state != CONSOLE_OPENED) {
    return;
  }

  if (IsKeyPressed(KEY_UP)) {
    g_console.hist.index = g_console.hist.index < g_console.hist.used
                               ? g_console.hist.index + 1
                               : g_console.hist.used;
    buf_cpy(g_console.text, g_console.hist.buffer + g_console.hist.index);
  } else if (IsKeyPressed(KEY_DOWN)) {
    g_console.hist.index =
        g_console.hist.index > 0 ? g_console.hist.index - 1 : 0;
    buf_cpy(g_console.text, g_console.hist.buffer + g_console.hist.index);
  }

  if (IsKeyPressed(KEY_ENTER) &&
      !buf_equal(g_console.hist.buffer, g_console.text)) {
    buf_cpy(g_console.hist.buffer, g_console.text);
    buf_array_shift_up(g_console.hist.buffer, N_LINES);
    g_console.hist.used =
        g_console.hist.used < N_LINES ? g_console.hist.used + 1 : (N_LINES - 1);
  }

  if (IsKeyPressed(KEY_ENTER)) {
    buf_array_shift_up(g_console.text, N_LINES);
    console_scan();
    buf_reset(g_console.text);

    g_console.hist.index = 0;
  }

  if (IsKeyPressed(KEY_BACKSPACE)) {
    g_console.backspace.down = true;
    if (g_console.text[0].used > 0) {
      g_console.text[0].chs[--(g_console.text[0].used)] = '\0';
    }
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
      g_console.text[0].chs[--(g_console.text[0].used)] = '\0';
      g_console.backspace.timer = 0.f;
      g_console.backspace.timeout = BACKSPACE_DELETE;
    } else if (g_console.text[0].used == 0) {
      g_console.backspace.down = false;
    }
  }

  int c = GetCharPressed();
  if (c != 0) {
    buf_put_char(&g_console.text[0], (char)c);
  }
}

void console_render() {
  DrawRectangleRec(g_console.window, g_console.background_color);

  DrawTextEx(g_console.font, g_console.text[0].chs,
               (Vector2){.x = 0, .y = (g_console.window.y + g_console.window.height) - (g_console.font_size + 2.f)},
               g_console.font_size, 1.2f, g_console.font_color);
/*
  g_console.text[0].chs[g_console.text[0].used] = '_';
  if (g_console.text[0].used < (LINE_SIZE - 1)) {
    g_console.text[0].chs[g_console.text[0].used + 1] = '\0';
  }
*/
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

bool console_is_active() { return g_console.anim.state == CONSOLE_OPENED; }

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
