#include <pwd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>

#include "parse.h"
#include "history.h"
#include "builtin.h"

#define ARRLEN(a) (sizeof(a)/sizeof(*a))


int fn_exit(arg*);
int cd(arg*);
int fn_history(arg*);


static builtin * builtins[] = {
	&(builtin){"exit", fn_exit, false},
	&(builtin){"cd", cd, false},
	&(builtin){"history", fn_history, false},
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
