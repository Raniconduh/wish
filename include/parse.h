#ifndef PARSE_H
#define PARSE_H

#include <stdbool.h>


typedef enum {
	TOK_PIPE,
	TOK_REDIR_IN,
	TOK_REDIR_OUT,
	TOK_REDIR_OUT_APPEND,
	TOK_HEREDOC,
	TOK_BG,
	TOK_STR,
} sh_tok;


typedef struct {
	sh_tok t_type;
	char * dat;
	size_t skip;
} token;


typedef struct _Arg {
	struct _Arg * next;
	char * dat;
} arg;


typedef struct {
	arg * args; // name will be arg 0
	char * redir_in;
	char * redir_out;
} cmd;


cmd * parse(const char *);
void destroy_cmd(cmd *);
bool isnumber(const char *);

#endif /* PARSE_H */
