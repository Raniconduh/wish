#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>

#include "io.h"
#include "keys.h"
#include "parse.h"
#include "history.h"
#include "builtin.h"

#define READLINE_BUFFER_LEN 256

#define PIPE_R(p) (p[0])
#define PIPE_W(p) (p[1])


static char * argv0;


char * readline(void) {
	tsetraw();
	
	char * s = malloc(READLINE_BUFFER_LEN + 1);
	size_t l = 0;
	size_t maxl = READLINE_BUFFER_LEN;

	int hist_cnt = -1;
	bool esc = false;

	int c;
	while ((c = getchar()) != EOF && c != '\n') {
		switch (c) {
		case KEY_DEL:
		case KEY_BACKSPACE:
			if (!l) break;
			fputs("\b \b", stdout);
			l--;
			break;
		case KEY_CTRL_D:
			if (l > 0) break;

			free(s);
			return NULL;
		case KEY_CTRL_C:
			s[0] = '\0';
			goto input_done;
		case KEY_ESC:
			esc = true;
			break;
		case '[':
			if (esc) {
				int c;
				switch ((c = getchar())) {
				case 'A': // keyup
					{
					char * ts = history_get(hist_cnt + 1);
					if (ts == NULL) break;

					free(s);
					l = strlen(ts);
					maxl = l + READLINE_BUFFER_LEN;
					s = malloc(l + READLINE_BUFFER_LEN + 1);
					strcpy(s, ts);
					hist_cnt++;
					}

					clrline();
					print_prompt();
					fputs(s, stdout);
					break;
				case 'B': // keydown
					if (hist_cnt == -1) break;

					if (hist_cnt == 0) {
						s = realloc(s, READLINE_BUFFER_LEN + 1);
						*s = '\0';
						l = 0;
						maxl = READLINE_BUFFER_LEN;
						hist_cnt--;
					} else {
						char * ts = history_get(hist_cnt - 1);
						if (ts == NULL) break;

						free(s);
						l = strlen(ts);
						maxl = l + READLINE_BUFFER_LEN;
						s = malloc(l + READLINE_BUFFER_LEN + 1);
						strcpy(s, ts);
						hist_cnt--;
					}

					clrline();
					print_prompt();
					fputs(s, stdout);
					break;
				case 'C': // keyright
				case 'D': // keyleft
				case 'F': // end
				case 'H': // home
					break;
				default:
					ungetc(c, stdin);
					break;
				}
				break;
			}
		default:
			putchar(c);

			s[l++] = c;
			if (l + 1 == maxl) {
				s = realloc(s, l + READLINE_BUFFER_LEN + 1);
				maxl += READLINE_BUFFER_LEN;
			}
			break;
		}
		
		if (esc && c != KEY_ESC) esc = false;
	}

	input_done:
	treset();

	if (c == EOF) {
		free(s);
		return NULL;
	}

	putchar('\n');

	s[l] = '\0';
	// don't remember empty input
	if (l) history_add(s);
	return s;
}


void sherror(cmd * com, int errnum) {
	switch (errnum) {
		case ENOENT:
			printf("%s: Command not found: %s\n", argv0, com->args->dat);
			break;
		default:
			printf("%s: %s\n", argv0, strerror(errnum));
			break;
	}
}


void openerror(char * f, int errnum) {
	switch (errnum) {
		case EDQUOT:
			printf("%s: Could not create file: %s:"
					"Filesystem quote reached\n", argv0, f);
			break;
		case EMFILE:
		case ENFILE:
			printf("%s: Cannot open file: %s:"
					"File descriptor limit reached\n", argv0, f);
			break;
		default:
			printf("%s: %s: %s\n", argv0, strerror(errnum), f);
			break;
	}
}


// return null-terminated array for exec family functions
char ** argarr(arg * args) {
	size_t cnt = 0;
	for (arg * a = args; a; a = a->next) cnt++;
	char ** arr = malloc(sizeof(char*) * (cnt + 1));
	char ** p = arr;
	for (arg * a = args; a; a = a->next) {
		*p++ = a->dat;
	}
	*p = NULL;
	return arr;
}


void sigint_handle(int _) {
	/* do nothing... */
}


int main(int argc, char ** argv) {
	argv0 = argv[0];

	sigaction(SIGINT, &(struct sigaction){
			.sa_handler = sigint_handle,
			.sa_flags = SA_RESTART}, NULL);

	while (true) {
		print_prompt();

		char * s = readline();
		if (!s) exit(0);
		else if (!*s) continue;

		pipeline * pl = parse(s);
		free(s);

		if (pl->len > 1) {
			// the last pipeline member does not need to create a pipe
			for (pipeline * l = pl; l->next; l = l->next) {
				int pfds[2];
				pipe(pfds);
				l->com->pipefdw = PIPE_W(pfds);
				l->next->com->pipefdr = PIPE_R(pfds);

				l->com->closepipe1 = PIPE_R(pfds);
				l->next->com->closepipe2 = PIPE_W(pfds);
			}
		}

		for (pipeline * l = pl; l; l = l->next) {
			cmd * com = l->com;

			if (redirect_input(com) == -1) {
				openerror(com->redir_in, errno);
				destroy_cmd(com);
				continue;
			}

			if (redirect_output(com) == -1) {
				openerror(com->redir_out, errno);
				destroy_cmd(com);
				continue;
			}

			// redirection takes precedence over pipes
			if (!com->redir_in) pipe_input(com);
			if (!com->redir_out) pipe_output(com);

			builtin * bi = check_builtin(com->args->dat);
			if (bi && !bi->forkme) {
				bi->fn(com->args);
			} else if (!fork()) {
				close(com->closepipe1);
				close(com->closepipe2);

				if (bi) {
					bi->fn(com->args);
				} else {
					// this is automatically freed
					char ** args = argarr(com->args);
					execvp(args[0], args);
				}

				// on error ....
				int errnum = errno;

				restore_io();
				sherror(com, errnum);
				exit(1);
			}

			// we are in parent proc here
			close(com->pipefdr);
			close(com->pipefdw);

			// restore file descriptor backups
			restore_io();

			destroy_cmd(com);
		}

		// need to wait for each command in the pipeline
		for (int i = 0; i < pl->len; i++) {
			wait(NULL);
		}

		destroy_pipeline(pl);
	}
}
