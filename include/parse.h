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
	TOK_VAR,
	TOK_VAREQ,
} sh_tok;


typedef struct {
	sh_tok t_type;
	char * dat;
	size_t skip;
	bool alone;
} token;


typedef struct arg {
	struct arg * next;
	char * dat;
} arg;


typedef struct cvar {
	struct cvar * next;
	struct cvar * last;
	char * var;
	char * val;
	bool env;
} cvar;


typedef struct {
	arg * args; // name will be arg 0
	cvar * vars;
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
cvar * new_cvar(void);
cvar * cvar_append(cvar **);

#endif /* PARSE_H */
