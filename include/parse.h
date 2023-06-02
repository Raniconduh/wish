#ifndef PARSE_H
#define PARSE_H

#include <stdbool.h>


typedef enum {
	TOK_NONE,
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


typedef struct arg {
	struct arg * next;
	char * dat;
} arg;


typedef struct {
	arg * args; // name will be arg 0
	char * redir_in;
	char * redir_out;
	int pipefdr;
	int pipefdw;
	int closepipe1;
	int closepipe2;
} cmd;


typedef struct pipeline {
	struct pipeline * next;
	cmd * com;
	int len;
} pipeline;


pipeline * parse(const char *);
void destroy_cmd(cmd *);
void destroy_pipeline(pipeline *);
bool isnumber(const char *);

#endif /* PARSE_H */
