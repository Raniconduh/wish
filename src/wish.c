#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <stdbool.h>
#include <sys/wait.h>

#include "keys.h"
#include "parse.h"
#include "builtin.h"

#define READLINE_BUFFER_LEN 256
#define TIOS_ATTR (ICANON | ECHO | ISIG)


char * argv0;


char * readline(void) {
	// let's disable canonical mode
	struct termios t;
	tcgetattr(STDIN_FILENO, &t);
	t.c_lflag &= ~TIOS_ATTR;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
	
	char * s = malloc(READLINE_BUFFER_LEN);
	size_t l = 0;

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
				case 'B': // keydown
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
			if ((l + 1) % READLINE_BUFFER_LEN == 0) {
				s = realloc(s, l + READLINE_BUFFER_LEN + 1);
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
		fputs("> ", stdout);

		char * s = readline();
		if (!s) exit(0);
		else if (!*s) continue;

		cmd * com = parse(s);

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

		int in_back = -1;
		int out_back = -1;

		// deal with fd redirection later			
		if (com->redir_in) {
			int infd = open(com->redir_in, O_RDONLY);
			if (infd == -1) {
				openerror(com->redir_in, errno);
				destroy_cmd(com);
				continue;
			}
			in_back = dup(STDIN_FILENO);
			dup2(infd, STDIN_FILENO);
		}

		if (com->redir_out) {
			// rw-r--r--
			int outfd = open(com->redir_out, O_CREAT | O_TRUNC | O_WRONLY, 0644);
			if (outfd == -1) {
				openerror(com->redir_out, errno);
				destroy_cmd(com);
				continue;
			}
			out_back = dup(STDOUT_FILENO);
			dup2(outfd, STDOUT_FILENO);
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
		if (in_back > -1) {
			close(STDIN_FILENO);
			dup2(in_back, STDIN_FILENO);
		}

		if (out_back > -1) {
			close(STDOUT_FILENO);
			dup2(out_back, STDOUT_FILENO);
		}
	}
}
