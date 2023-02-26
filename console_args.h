
#ifndef _HEADER_FILE_console_args_20230226003325_
#define _HEADER_FILE_console_args_20230226003325_

struct console_arg_iter {
  int *chrs;
  int chr_count;

  int next;
};

struct console_arg {
  int *start;
  int size;
};

struct console_arg_iter console_arg_iter_init(int *chs, int count);

int console_arg_iter_next_arg(struct console_arg_iter *, struct console_arg *arg);

int console_arg_iter_count_args(struct console_arg_iter const *);

#endif