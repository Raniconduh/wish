#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "parse.h"
#include "str.h"


static cmd * new_com(void) {
	cmd * com = malloc(sizeof(cmd));
	com->args = NULL;
	com->redir_in = NULL;
	com->redir_out = NULL;
	return com;
}


static arg * new_arg(void) {
	arg * a = malloc(sizeof(arg));
	a->next = NULL;
	a->dat = NULL;
	return a;
}


static token * new_token(void) {
	token * t = malloc(sizeof(token));
	t->t_type = TOK_STR;
	t->dat = NULL;
	t->skip = 0;
	return t;
}


static arg *  arg_append(arg ** a) {
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


token * parse_next_str(const char * s) {
	if (!*s) return NULL;


	str * tstr = new_str();

	token * tok = new_token();

	// single/double-quoted strings
	bool parse_ss = false;
	bool parse_ds = false;

	int seen_lt = 0; /* < */
	int seen_gt = 0; /* > */
	bool seen_bs = false; /* \ */
	
	bool seen_stuff = false;

	char * p = (char*)s;
	tok->skip += seek_start(&p);

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
			} else break;
		} else {
			switch (c) {
			case '\\': /* \ */
				if (parse_ss) {
					str_addc(tstr, '\\');
				} else if (seen_bs) {
					str_addc(tstr, '\\');
					seen_bs = false;
				} else seen_bs = true;
				break;
			case '\'': /* ' */
				if (seen_bs) {
					str_addc(tstr, '\'');
					seen_bs = false;
				} else if (parse_ss) {
					parse_ss = false;
				} else {
					parse_ss = true;
				}
				break;
			case '"':
				if (seen_bs) {
					str_addc(tstr, '"');
					seen_bs = false;
				} else if (parse_ds) {
					parse_ds = false;
				} else {
					parse_ds = true;
				}
				break;
			case '<':
				if (parse_ss || parse_ds) {
					str_addc(tstr, c);
				} else if (seen_bs) {
					str_addc(tstr, c);
					seen_bs = false;
				} else if (seen_stuff) {
					done = true;
				} else {
					seen_lt++;
				}
				break;
			case '>':
				if (parse_ss || parse_ds) {
					str_addc(tstr, c);
				} else if (seen_bs) {
					str_addc(tstr, c);
					seen_bs = false;
				} else if (seen_stuff) {
					done = true;
				} else {
					seen_gt++;
				}
				break;
			default:
				str_addc(tstr, c);
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
	} else if (!retl) { // empty string
		free(rets);
		free(tok);
		tok = NULL;
	} else {
		tok->t_type = TOK_STR;
	}

	if (tok) tok->dat = rets;
	return tok;
}


cmd * parse(const char * s) {
	char * sp = (char*)s;

	cmd * com = new_com();
	arg * args = NULL;

	bool waiting_redir_in = false;
	bool waiting_redir_out = false;

	token * t;
	while ((t = parse_next_str(sp))) {
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
		case TOK_STR:
		default:
			if (waiting_redir_in) {
				com->redir_in = t->dat;
				waiting_redir_in = false;
			} else if (waiting_redir_out) {
				com->redir_out = t->dat;
				waiting_redir_out = false;
			} else {
				arg * a = arg_append(&args);
				a->dat = t->dat;
			}
			break;
		}

		free(t);
	}

	com->args = args;
	return com;
}


void destroy_cmd(cmd * com) {
	for (arg * a = com->args; a;) {
		arg * next = a->next;
		free(a->dat);
		free(a);
		a = next;
	}

	if (com->redir_in) free(com->redir_in);
	if (com->redir_out) free(com->redir_out);

	free(com);
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
