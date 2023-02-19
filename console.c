#include "console.h"
#include <raylib.h>
#include <raymath.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N_LINES 255
#define LINE_SIZE 255
#define N_DECISIONS 255

#define BACKSPACE_DELETE_FIRST 0.5f
#define BACKSPACE_DELETE .025f

static inline bool is_white_space(int c) {
  return (int)('\n') == c || (int)(' ') == c || (int)('\t') == c ||
         (int)('\f') == c || (int)('\v') == c || (int)('\r') == c;
}

static inline bool is_quote(int c) { return (c == 34 || c == 39); }

void console_builtin_clear(int len, int *c);
void console_builtin_exit(int len, int *c);

struct decisions {
  const char *key[N_DECISIONS];
  void (*value[N_DECISIONS])(int, int *);
  int used;
};

struct buf {
  int chs[LINE_SIZE];
  int used;
};

void buf_cpy(struct buf *dst, struct buf *src) {
  dst->used = src->used;
  for (int i = 0; i < LINE_SIZE; ++i) {
    dst->chs[i] = src->chs[i];
  }
}

void buf_put_char(struct buf *buf, int c) {
  if (buf->used < LINE_SIZE) {
    buf->chs[buf->used++] = c;
  }
}

void buf_reset(struct buf *buf) { buf->used = 0; }

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

/*
 * Count the number of whitespace delimited arguments in a UTF-8 string.
 * characters the UTF-8 string in question.
 * count length of the UTF-8 string measured in max-length codepoints (e.g.: 32 bit integers)
 */
int console_count_args(int const *characters, int count) {
  int arg_count = 0;

  for (int i = 0, quote_level = 0; i < count; ++i) {
    int c = characters[i];
    if (is_white_space(c) && quote_level == 0) {
      arg_count++;
    } else if (is_quote(c)) {
      quote_level = (quote_level > 0) ? (quote_level - 1) : (quote_level + 1);
    }

    if (i == (count - 1) && quote_level == 0) {
      arg_count++;
    }
  }

  return arg_count;
}

void console_writeln(char const *blah) {
  struct buf *b = g_console.text;
  b->used = (int)strlen(blah);
  for (int i = 0; i < b->used; ++i) {
    b->chs[i] = (int)blah[i];
  }
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
  for (int i = 0; i < b->used; ++i) {
    b->chs[i] = (int)buffer[i];
  }

  va_end(args);

  buf_array_shift_up(g_console.text, N_LINES);
}

void console_register(const char *name, void (*f)(int, int *)) {
  int j = g_console.decisions.used;
  g_console.decisions.key[j] = name;
  g_console.decisions.value[j] = f;
  g_console.decisions.used++;
}

// scan command line and find out which command to run
void console_scan() {
  struct buf *prompt_line = g_console.text;
  if (prompt_line->used == 0) {
    return; // empty input
  }

  int prefix_end = 0, prefix_start = 0, prefix_len = 0;

  // eat prefix whitespace
  for (; prefix_start < prompt_line->used; ++prefix_start) {
    if (!is_white_space(prompt_line->chs[prefix_start])) {
      break;
    }
  }

  // find end of command verb
  for (prefix_end = prefix_start; prefix_end < prompt_line->used;
       ++prefix_end) {
    if (is_white_space(prompt_line->chs[prefix_end])) {
      break;
    }
  }
  prefix_len = (prefix_end - prefix_start);

  for (int decision_index = 0; decision_index < g_console.decisions.used;
       ++decision_index) {
    bool prefix_match = true;

    const char *key = g_console.decisions.key[decision_index];
    size_t key_length = strlen(key);
    if (key_length < prefix_len) {
      continue;
    }

    for (int i = prefix_start; i < prefix_end && i < prompt_line->used; i++) {
      if (prompt_line->chs[i] != ((int)key[(i - prefix_start)])) {
        prefix_match = false;
        break;
      }
    }

    if (prefix_match) {
      int start_of_args = prefix_end;
      for (; start_of_args < prompt_line->used &&
             is_white_space(prompt_line->chs[start_of_args]);
           start_of_args++)
        ;

      (*g_console.decisions.value[decision_index])(
          prompt_line->used - start_of_args, prompt_line->chs + start_of_args);
      return;
    }
  }

  console_writeln("Error: No such command");
}

static inline void console_update_animation() {
  if (IsKeyPressed(g_console.key)) {
    if (g_console.anim.state == CONSOLE_CLOSED) {
      g_console.anim.state = CONSOLE_OPENING;
    } else if (g_console.anim.state == CONSOLE_OPENED) {
      g_console.anim.state = CONSOLE_CLOSING;
    }
  }

  if (g_console.anim.state == CONSOLE_OPENING) {
    g_console.anim.timer += GetFrameTime();
    if (g_console.anim.timer > 0.001f) {
      g_console.anim.percent = Clamp(g_console.anim.percent + 0.01f, 0.f, 1.f);
      g_console.anim.timer = 0.f;
    }
    g_console.window.height = Lerp(0.f, GetScreenHeight() / 3.f, g_console.anim.percent);

    if (g_console.anim.percent >= 1.f) {
      g_console.anim.state = CONSOLE_OPENED;
      g_console.anim.timer = 0.f;
    }
  } else if (g_console.anim.state == CONSOLE_CLOSING) {
    g_console.anim.timer += GetFrameTime();
    if (g_console.anim.timer > 0.001f) {
      g_console.anim.percent = Clamp(g_console.anim.percent - 0.01f, 0.f, 1.f);
      g_console.anim.timer = 0.f;
    }
    g_console.window.height = Lerp(0.f, GetScreenHeight() / 3.f, g_console.anim.percent);

    if (g_console.anim.percent <= 0.f) {
      g_console.anim.state = CONSOLE_CLOSED;
      g_console.anim.timer = 0.f;
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
      g_console.text[0].used--;
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
      g_console.text[0].used--;
      g_console.backspace.timer = 0.f;
      g_console.backspace.timeout = BACKSPACE_DELETE;
    } else if (g_console.text[0].used == 0) {
      g_console.backspace.down = false;
    }
  }

  int c = GetCharPressed();
  if (c != 0) {
    buf_put_char(&g_console.text[0], c);
  }
}

void console_render() {
  DrawRectangleRec(g_console.window, g_console.background_color);

  for (int i = 0; i < N_LINES; ++i) {
    float hn = (g_console.window.y + g_console.window.height) -
               ((g_console.font_size + 2.f) * (i + 1));

    int len = g_console.text[i].used;
    if (i == 0) {
      g_console.text[0].chs[g_console.text[i].used] = (int)'_';
      len = g_console.text[i].used + 1;
    }

    DrawTextCodepoints(g_console.font, g_console.text[i].chs, len,
                       (Vector2){.x = 0, .y = hn}, g_console.font_size, 1.5,
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

void console_builtin_exit(int len, int *c) {
  if (len > 1) {
    console_writeln("Error: command 'exit' does only take one argument");
    return;
  }

  exit(0);
}

void console_builtin_clear(int len, int *c) {
  if (len > 0) {
    console_writeln("Error: command 'clear' does not take any arguments");
    return;
  }

  for (int i = 0; i < N_LINES; ++i) {
    g_console.text[i].used = 0;
  }
}
