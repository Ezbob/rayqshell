
#include "console_args.h"
#include "console_config.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>


static inline bool is_white_space(char c) {
  return ('\n') == c || (' ') == c || ('\t') == c || ('\f') == c ||
         ('\v') == c || ('\r') == c;
}

static inline bool is_quote(char c) { return (c == '\"' || c == '\''); }

int console_arg_iter_count_args(struct console_arg_iter const *it) {
  int arg_count = 0;

  for (int i = 0, quote_level = 0; i < it->chr_count; ++i) {
    int c = it->chrs[i];
    if (is_white_space(c) && quote_level == 0) {
      arg_count++;
    } else if (is_quote(c)) {
      quote_level = (quote_level > 0) ? (quote_level - 1) : (quote_level + 1);
    }

    if (i == (it->chr_count - 1) && quote_level == 0) {
      arg_count++;
    }
  }

  return arg_count;
}

struct console_arg_iter console_arg_iter_init(char const *chs, int count) {
  return (struct console_arg_iter){.next = 0, .chrs = chs, .chr_count = count};
}

int console_arg_iter_next_arg(struct console_arg_iter *iter,
                              struct console_arg *arg) {
  if (!arg || !iter)
    return -1;

  enum t { QUOTED, UNQUOTED } arg_t = UNQUOTED;

  for (; iter->next < iter->chr_count; ++iter->next) {
    char c = iter->chrs[iter->next];
    if (!is_white_space(c)) {
      break;
    }
  }

  if (iter->next >= iter->chr_count) {
    return -1;
  }

  if (is_quote(iter->chrs[iter->next])) {
    arg_t = QUOTED;
    iter->next++;
  }

  int istart = iter->next;
  char const *start = iter->chrs + iter->next;
  int size = 0;

  if (arg_t == QUOTED) {
    int found_end_quote = -1;
    for (; iter->next < iter->chr_count; ++iter->next, size++) {
      char c = iter->chrs[iter->next];
      if (is_quote(c)) {
        found_end_quote = iter->next;
        break;
      }
    }

    if (found_end_quote == -1) {
      // unbounded quotation
      return -1;
    }

    for (; iter->next < iter->chr_count; ++iter->next) {
      char c = iter->chrs[iter->next];
      if (is_white_space(c)) {
        break; // position interator at next whitespace or at end of string
      }
    }

  } else if (arg_t == UNQUOTED) {

    for (; iter->next < iter->chr_count; ++iter->next, size++) {
      char c = iter->chrs[iter->next];
      if (is_white_space(c)) {
        break;
      } else if (is_quote(c)) {
        return -1;
      }
    }
  }

  arg->start = start;
  arg->size = size;
  return 0;
}

char const *console_arg_as_str(struct console_arg *a) {
  if (a->size == 0) {
    return NULL;
  }
  static char buffer[LINE_SIZE];

  if (!memcpy(buffer, a->start, a->size)) {
    return NULL;
  }

  buffer[a->size] = '\0';

  return buffer;
}