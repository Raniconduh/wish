#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <stdbool.h>
#include <sys/wait.h>

#include "io.h"
#include "keys.h"
#include "parse.h"
#include "history.h"
#include "builtin.h"

#define READLINE_BUFFER_LEN 256
#define TIOS_ATTR (ICANON | ECHO | ISIG)


static char * argv0;


char * readline(void) {
	// let's disable canonical mode
	struct termios t;
	tcgetattr(STDIN_FILENO, &t);
	t.c_lflag &= ~TIOS_ATTR;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
	
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
	// and... reenable it
	t.c_lflag |= TIOS_ATTR;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);

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
	char ** arr = malloc(sizeof(char*) * cnt + 1);
	char ** p = arr;
	for (arg * a = args; a; a = a->next) {
		*p++ = a->dat;
	}
	*p = NULL;
	return arr;
}


int main(int argc, char ** argv) {
	argv0 = argv[0];

	while (true) {
		print_prompt();

		char * s = readline();
		if (!s) exit(0);
		else if (!*s) continue;

		cmd * com = parse(s);
		free(s);

/*
		// TEST CODE
		printf("REDIR INPUT: %s\n", com->redir_in);
		printf("REDIR OUTPUT: %s\n", com->redir_out);
		puts("ARGS:");
		int i = 0;
		for (arg * a = com->args; a; a = a->next, i++) {
			printf("%d: %s\n", i, a->dat);
		}
*/

		// deal with fd redirection later
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

		builtin * bi = check_builtin(com->args->dat);
		if (bi && !bi->forkme) {
			bi->fn(com->args);	
		} else if (!fork()) {
			if (bi) {
				bi->fn(com->args);	
			} else {
				// this is automatically freed
				char ** args = argarr(com->args);
				if (execvp(args[0], args) < 0) {
					restore_io();
					sherror(com, errno);
				}
				exit(1);
			}
		} else {
			// we can do this concurrently
			destroy_cmd(com);
			wait(NULL);
		}

		// restore file descriptor backups
		restore_io();
	}
}
