
#include "rqshell_args.h"
#include "rqshell_config.h"
#include "raylib.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static inline bool is_quote(char c) { return (c == '\"' || c == '\''); }

static inline int parse_fields(struct rqshell_arg_iter *iter, char **output, int max_size) {
  if (!iter || !output) {
    return -1;
  }

  enum t { QUOTED, UNQUOTED } arg_t = UNQUOTED;
  *output = NULL;

  for (; iter->next < iter->chr_count; ++iter->next) {
    char c = iter->chrs[iter->next];
    if (!isspace((unsigned int)c)) {
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
      if (isspace((unsigned int)c)) {
        break; // position interator at next whitespace or at end of string
      }
    }

  } else if (arg_t == UNQUOTED) {

    for (; iter->next < iter->chr_count; ++iter->next, size++) {
      char c = iter->chrs[iter->next];
      if (isspace((unsigned int)c)) {
        break;
      } else if (is_quote(c)) {
        return -1;
      }
    }
  }

  if (size >= max_size) {
    return -1;
  }

  *output = ((char *)start);
  return size;
}

int rqshell_arg_iter_count_args(struct rqshell_arg_iter const *it) {
  int arg_count = 0;

  for (int i = 0, quote_level = 0; i < it->chr_count; ++i) {
    int c = it->chrs[i];
    if (isspace((unsigned int)c) && quote_level == 0) {
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

struct rqshell_arg_iter rqshell_arg_iter_init(char const *chs, int count) {
  return (struct rqshell_arg_iter){.next = 0, .chrs = chs, .chr_count = count};
}

const char *rqshell_arg_iter_next(struct rqshell_arg_iter *iter) {
  static char buffer[LINE_SIZE];
  char *start = NULL;
  int size = parse_fields(iter, &start, LINE_SIZE);
  if (size < 0) {
    return NULL;
  }
  if (!memcpy(buffer, start, size)) {
    return NULL;
  }
  buffer[size] = '\0';
  return buffer;
}
