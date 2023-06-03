#include <pwd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>

#include "var.h"
#include "wish.h"
#include "parse.h"
#include "history.h"
#include "builtin.h"

#define ARRLEN(a) (sizeof(a)/sizeof(*a))


extern char ** environ;

int fn_exit(arg*);
int cd(arg*);
int fn_history(arg*);
int fn_export(arg*);
int fn_set(arg*);


static builtin * builtins[] = {
	&(builtin){"exit", fn_exit, false},
	&(builtin){"cd", cd, false},
	&(builtin){"history", fn_history, false},
	&(builtin){"export", fn_export, false},
	&(builtin){"set", fn_set, false},
};


builtin * check_builtin(const char * s) {
	for (size_t i = 0; i < ARRLEN(builtins); i++) {
		if (!strcmp(s, builtins[i]->name))
			return builtins[i];
	}
	return NULL;
}


int fn_exit(arg * args) {
	char ex_num = 0;
	if (args->next) {
		ex_num = strtod(args->next->dat, NULL);
	}
	exit(ex_num);
}


int cd(arg * args) {
	static char back[1024] = {0};
	if (!*back) getcwd(back, sizeof(back));

	bool cdback = false;

	char * d = NULL;
	if (args->next) {
		d = args->next->dat;
		if (!strcmp(d, "-")) {
			cdback = true;
			d = back;
		}
	}

	if (!d) {
		// cd home
		d = getenv("HOME");
		if (!d || !*d) {
			struct passwd * pw = getpwuid(geteuid());
			d = pw->pw_dir;
		}
	}

	if (!cdback) getcwd(back, sizeof(back));

	if (chdir(d) < 0) {
		printf("cd: %s\n", strerror(errno));
		return 1;
	}

	setenv("PWD", d, 1);
	return 0;
}


int fn_history(arg * args) {
	int start = 0;
	bool nostart = false;

	if (args->next && isnumber(args->next->dat)) {
		start = strtod(args->next->dat, NULL);
		if (start == 0 || start >= history.entries || -start >= history.entries)
			nostart = true;
	} else {
		nostart = true;
	}

	#define HFMTPF(...) printf("[%d] %s\n", i + 1, h->dat);
	if (nostart) {
		he * h = history.last;
		for (int i = 0; h; h = h->prev, i++) {
			HFMTPF();
		}
	} else if (start < 0) {
		he * h = history.hist;
		for (int i = 0; i > start; i--) {
			h = h->next;
		}

		for (int i = history.entries + start - 1; h->prev; h = h->prev, i++) {
			HFMTPF()
		}
	} else {
		he * h = history.last;
		for (int i = 0; i < start - 1; i++) {
			h = h->prev;
		}

		for (int i = start - 1; h; h = h->prev, i++) {
			HFMTPF();
		}
	}

	return 0;
}


static int envcomp(const void * a, const void * b) {
	return strcmp(*(char**)a, *(char**)b);
}


int fn_export(arg * args) {
	for (arg * a = args->next; a; a = a->next) {
		char * vpair[2];
		var_parse(a->dat, vpair);
		if (isspecial(*V_NAME(vpair))) {
			fputs("export: Invalid variable format", stderr);
		} else {
			if (!*V_VAL(vpair)) {
				char * prev = var_expand(V_NAME(vpair));
				var_set(V_NAME(vpair), strdup(prev), true);
			} else {
				var_set(V_NAME(vpair), V_VAL(vpair), true);
			}
		}
	}

	// no args given, print env variables
	// let's waste as much cpu and memory as possible:
	if (!args->next) {
		size_t envlen = 0;
		for (char ** p = environ; *p; p++) envlen++;

		char ** envcopy = malloc(sizeof(char*) * envlen);
		for (size_t i = 0; i < envlen; i++) {
			envcopy[i] = strdup(environ[i]);
		}
		qsort(envcopy, envlen, sizeof(char*), envcomp);
		for (size_t i = 0; i < envlen; i++) {
			printf("export %s\n", envcopy[i]);
			free(envcopy[i]);
		}
		free(envcopy);
	}

	return 0;
}


int fn_set(arg * args) {
	var * v;
	while ((v = var_walk())) {
		printf("%s=%s\n", v->var, v->val);
	}
	return 0;
}
