
#ifndef _HEADER_FILE_console_args_20230226003325_
#define _HEADER_FILE_console_args_20230226003325_

struct console_arg_iter {
  char const * chrs;
  int chr_count;

  int next;
};

struct console_arg {
  char const *start;
  int size;
};

struct console_arg_iter console_arg_iter_init(char const * chs, int count);

int console_arg_iter_next_arg(struct console_arg_iter *, struct console_arg *arg);

int console_arg_iter_count_args(struct console_arg_iter const *);

char *console_arg_copy_as_cstring(struct console_arg *);

#endif