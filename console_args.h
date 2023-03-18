
#ifndef _HEADER_FILE_console_args_20230226003325_
#define _HEADER_FILE_console_args_20230226003325_

/*
 * Console argument iterator.
 * An iterator that can parse a raw argument line into sub strings called fields.
 * These fields are either a sequence of non-whitespace characters or a sequence begging
 * with a quote character (single or double) and closing with a identical quote character.
 */
struct console_arg_iter {
  char const *chrs; // reference to the original argument line
  int chr_count; // the character count of the original command line
  int next; // current place in the buffer
};

/*
 * Create an iterator from command raw characters.
 */
struct console_arg_iter console_arg_iter_init(char const *chs, int count);


/*
 * Get next argument or a null pointer
 */
const char *console_arg_iter_next(struct console_arg_iter *);

/*
 * Count the number of arguments that can be extracted from the iterator.
 */
int console_arg_iter_count_args(struct console_arg_iter const *);

#endif