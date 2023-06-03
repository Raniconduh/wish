#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "str.h"
#include "var.h"
#include "wish.h"
#include "parse.h"


static cmd * new_com(void) {
	cmd * com = malloc(sizeof(cmd));
	com->args = NULL;
	com->redir_in = NULL;
	com->redir_out = NULL;
	com->pipefdr = -1;
	com->pipefdw = -1;
	com->closepipe1 = -1;
	com->closepipe2 = -1;
	return com;
}


static arg * new_arg(void) {
	arg * a = malloc(sizeof(arg));
	a->next = NULL;
	a->dat = NULL;
	return a;
}

static arg * arg_append(arg ** a) {
	if (!*a) {
		*a = new_arg();
		return *a;
	}

	arg * l;
	for (l = *a; l->next; l = l->next)
		;

	l->next = new_arg();
	return l->next;
}


static pipeline * new_pipeline(void) {
	pipeline * pl = malloc(sizeof(pipeline));
	pl->next = NULL;
	pl->com = NULL;
	pl->len = 0;
	return pl;
}


static pipeline * pipeline_append(pipeline ** pl) {
	if (!*pl) {
		*pl = new_pipeline();
		(*pl)->len++;
		return *pl;
	}

	(*pl)->len++;

	pipeline * l;
	for (l = *pl; l->next; l = l->next)
		;

	l->next = new_pipeline();
	return l->next;
}


static token * new_token(void) {
	token * t = malloc(sizeof(token));
	t->t_type = TOK_NONE;
	t->dat = NULL;
	t->skip = 0;
	t->alone = false;
	return t;
}


cvar * new_cvar(void) {
	cvar * v = malloc(sizeof(cvar));
	v->next = NULL;
	v->last = NULL;
	v->var = NULL;
	v->val = NULL;
	v->env = false;
	return v;
}


cvar * cvar_append(cvar ** v) {
	if (!*v) {
		*v = new_cvar();
		(*v)->last = (*v);
		return *v;
	}

	cvar * last = (*v)->last;
	last->next = new_cvar();
	(*v)->last = last->next;
	return (*v)->last;
}


// go to first non-space character
static size_t seek_start(char ** s) {
	size_t l = 0;
	char * p;
	for (p = *s; *p; p++) {
		if (!isspace(*p)) break;
		l++;
	}
	*s = p;
	return l;
}


void parse_error(const char * s) {
	fprintf(stderr, "%s: %s\n", argv0, s);
}


token * parse_next_str(const char * s) {
	if (!*s) return NULL;

	str * tstr = new_str();

	token * tok = new_token();

	// single/double-quoted strings
	bool parse_ss = false;
	bool parse_ds = false;

	// used when ${VAR} is given
	bool parse_vbrack = false;
	bool parse_var = false;

	int seen_lt = 0; /* < */
	int seen_gt = 0; /* > */
	bool seen_bs = false; /* \ */
	
	bool seen_stuff = false;

	char * p = (char*)s;
	tok->skip = seek_start(&p);
	if (tok->skip > 0) {
		tok->alone = true;
	} else tok->alone = false;

	bool done = false;
	for (; *p && !done; p++) {
		char c = *p;
		tok->skip++;

		if (isspace(c)) {
			if (parse_ss || parse_ds) {
				str_addc(tstr, c);
			} else if (seen_bs) {
				str_addc(tstr, c);
				seen_bs = false;
			} else if (!seen_stuff) {
				continue;
			} else {
				tok->skip--;
				break;
			}
		} else {
			switch (c) {
			case '\\': /* \ */
				if (parse_ss) {
					goto lex_reg;
				} else if (seen_bs) {
					seen_bs = false;
					goto lex_reg;
				} else seen_bs = true;
				break;
			case '\'': /* ' */
				if (seen_bs) {
					seen_bs = false;
					goto lex_reg;
				} else if (parse_ss) {
					parse_ss = false;
				} else {
					parse_ss = true;
				}
				break;
			case '"':
				if (seen_bs) {
					seen_bs = false;
					goto lex_reg;
				} else if (parse_ds) {
					parse_ds = false;
				} else {
					parse_ds = true;
				}
				break;
			case '<':
				if (parse_ss || parse_ds) {
					goto lex_reg;
				} else if (seen_bs) {
					seen_bs = false;
					goto lex_reg;
				} else if (seen_stuff) {
					done = true;
				} else {
					seen_lt++;
				}
				break;
			case '>':
				if (parse_ss || parse_ds) {
					goto lex_reg;
				} else if (seen_bs) {
					seen_bs = false;
					goto lex_reg;
				} else if (seen_stuff) {
					done = true;
				} else {
					seen_gt++;
				}
				break;
			case '|':
				if (parse_ss || parse_ds) {
					goto lex_reg;
				} else if (seen_bs) {
					seen_bs = false;
					goto lex_reg;
				} else if (!seen_stuff) {
					tok->t_type = TOK_PIPE;
					tok->skip++;
					done = true;
				} else {
					done = true;
				}
				break;
			case '$':
				if (parse_ss) {
					goto lex_reg;
				} else if (seen_bs) {
					seen_bs = false;
					goto lex_reg;
				} else if (!seen_stuff) {
					parse_var = true;
				} else {
					done = true;
				}
				break;
			case '{':
				if (!parse_var) goto lex_reg;
				parse_vbrack = true;
				break;
			case '}':
				if (!parse_vbrack) goto lex_reg;
				tok->skip++;
				done = true;
				break;
			case '=':
				if (parse_ss || parse_ds) {
					goto lex_reg;
				} else if (seen_bs) {
					seen_bs = false;
					goto lex_reg;
				} else if (seen_stuff && !tok->alone) {
					done = true;
					tok->skip++;
					tok->t_type = TOK_VAREQ;
				} else {
					goto lex_reg;
				}
				break;
			default:
			lex_reg:
				if (parse_var && isspecial(c)) {
					done = true;
				} else {
					str_addc(tstr, c);
				}
				break;
			}

			seen_stuff = true;
		}
	}

	if (done) tok->skip--;

	size_t retl = str_len(tstr);
	char * rets = str_conv(tstr);

	if (seen_lt) {
		switch (seen_lt) {
		case 1: // <
			tok->t_type = TOK_REDIR_IN;
			break;
		case 2: // <<
			tok->t_type = TOK_HEREDOC;
			break;
		default:
			break;
		}

		if (!retl) {
			free(rets);
			rets = NULL;
		}
	} else if (seen_gt) {
		switch (seen_gt) {
		case 1: // >
			tok->t_type = TOK_REDIR_OUT;
			break;
		case 2: // >>
			tok->t_type = TOK_REDIR_OUT_APPEND;
			break;
		default:
			break;
		}

		if (!retl) {
			free(rets);
			rets = NULL;
		}
	} else if (parse_var) {
		tok->t_type = TOK_VAR;
	} else if (tok->t_type == TOK_NONE) {
		if (!retl) { // empty string
			free(rets);
			free(tok);
			tok = NULL;
		} else {
			tok->t_type = TOK_STR;
		}
	}

	if (tok) tok->dat = rets;
	return tok;
}


pipeline * parse(const char * s) {
	char * sp = (char*)s;

	pipeline * pl = NULL;

	bool done = false;
	while (!done) {
		cmd * com = new_com();
		arg * args = NULL;
		cvar * vars = NULL;

		bool waiting_redir_in = false;
		bool waiting_redir_out = false;
		bool seen_pipe = false;
		bool seen_eq = false;
		bool seen_com = false;

		char * vname = NULL;

		arg * prev = NULL;
		cvar * vprev = NULL;
		bool prev_is_arg = false;

		token * t;
		while (!seen_pipe && (t = parse_next_str(sp))) {
			sp += t->skip;

			switch (t->t_type) {
			case TOK_REDIR_IN:
				if (t->dat) com->redir_in = t->dat;
				else waiting_redir_in = true;
				break;
			case TOK_REDIR_OUT:
				if (t->dat) com->redir_out = t->dat;
				else waiting_redir_out = true;
				break;
			case TOK_PIPE:
				seen_pipe = true;
				break;
			case TOK_VAREQ:
				if (seen_com) goto parse_reg;
				seen_eq = true;
				vname = t->dat;
				break;
			case TOK_VAR:
				{
				char * e = var_expand(t->dat);
				if (!e || !*e) continue;
				free(t->dat);
				t->dat = strdup(e);
				}
			case TOK_STR:
			default:
			parse_reg:
				if (seen_eq && t->alone) {
					cvar * v = cvar_append(&vars);
					v->var = vname;
					v->val = strdup("");

					vprev = v;
					prev_is_arg = false;
					seen_eq = false;
				}

				if (waiting_redir_in) {
					com->redir_in = t->dat;
					waiting_redir_in = false;
				} else if (waiting_redir_out) {
					com->redir_out = t->dat;
					waiting_redir_out = false;
				} else if (seen_eq) {
					char * val = NULL;
					if (!t->alone) val = t->dat;
					else val = strdup("");
					cvar * v = cvar_append(&vars);
					v->var = vname;
					v->val = val;

					vprev = v;
					prev_is_arg = false;
					seen_eq = false;
				} else if (!prev || (t->alone && prev_is_arg)) {
					seen_com = true;
					arg * a = arg_append(&args);
					a->dat = t->dat;

					prev = a;
					prev_is_arg = true;
				} else if (prev_is_arg) {
					seen_com = true;
					char * p = prev->dat;
					prev->dat = malloc(strlen(prev->dat) + strlen(t->dat) + 1);
					strcpy(prev->dat, p);
					strcat(prev->dat, t->dat);
					free(p);
					free(t->dat);
				} else {
					char * p = vprev->val;
					vprev->val = malloc(strlen(vprev->val) + strlen(t->dat) + 1);
					strcpy(vprev->val, p);
					strcat(vprev->val, t->dat);
					free(p);
					free(t->dat);
				}
				break;
			}

			free(t);
		}

		// no value, set to empty
		if (seen_eq) {
			cvar * v = cvar_append(&vars);
			v->var = vname;
			v->val = strdup("");
		}

		com->args = args;
		com->vars = vars;

		if (pl && pl->len && !args && vars) {
			parse_error("Invalid variable definition");
			for (cvar * v = vars; v; v = v->next) {
				free(v->var);
				free(v->val);
			}
			destroy_cmd(com);
			continue;
		}

		pipeline * l = pipeline_append(&pl);
		l->com = com;

		if (!t) done = true;
	}

	return pl;
}


void destroy_cmd(cmd * com) {
	for (arg * a = com->args; a;) {
		arg * next = a->next;
		free(a->dat);
		free(a);
		a = next;
	}

	for (cvar * v = com->vars; v;) {
		cvar * next = v->next;
		free(v);
		v = next;
	}

	if (com->redir_in) free(com->redir_in);
	if (com->redir_out) free(com->redir_out);

	free(com);
}


void destroy_pipeline(pipeline * pl) {
	for (pipeline * l = pl; l;) {
		pipeline * next = l->next;
		free(l);
		l = next;
	}
}


bool isnumber(const char * s) {
	char * p = (char*)s;

	if (!*s) return false;

	if (*p == '-') p++;
	if (!*p) return false;

	for (; *p; p++) {
		if (!isdigit(*p)) return false;
	}
	return true;
}
