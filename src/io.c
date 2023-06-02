#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>

#include "io.h"

#define TIOS_ATTR (ICANON | ECHO | ISIG)


static struct termios tback;

static int stdin_back = -1;
static int stdout_back = -1;


int redirect_input(cmd * com) {
	if (!com->redir_in) return 0;

	restore_input();

	int infd = open(com->redir_in, O_RDONLY);
	if (infd == -1) return -1;
	stdin_back = dup(STDIN_FILENO);
	dup2(infd, STDIN_FILENO);

	return 0;
}


int redirect_output(cmd * com) {
	if (!com->redir_out) return 0;

	restore_output();

	int outfd = open(com->redir_out, O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (outfd == -1) return -1;
	stdout_back = dup(STDOUT_FILENO);
	dup2(outfd, STDOUT_FILENO);

	return 0;
}


void restore_input(void) {
	if (stdin_back == -1) return;
	dup2(stdin_back, STDIN_FILENO);
	stdin_back = -1;
}


void restore_output(void) {
	if (stdout_back == -1) return;
	dup2(stdout_back, STDOUT_FILENO);
	stdout_back = -1;
}


// restores from both pipes and redirections
void restore_io(void) {
	restore_input();
	restore_output();
}


void pipe_input(cmd * com) {
	if (com->pipefdr == -1) return;

	restore_input();

	stdin_back = dup(STDIN_FILENO);
	dup2(com->pipefdr, STDIN_FILENO);
}


void pipe_output(cmd * com) {
	if (com->pipefdw == -1) return;

	restore_output();

	stdout_back = dup(STDOUT_FILENO);
	dup2(com->pipefdw, STDOUT_FILENO);
}


void tsetraw(void) {
	tcgetattr(STDIN_FILENO, &tback);
	struct termios t = tback;
	t.c_lflag &= ~TIOS_ATTR;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);

}


void treset(void) {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &tback);
}


void getyx(int yx[2]) {
	tsetraw();
	// get cursor position
	fputs("\033[6n", stdout);
	// response is in the format ^[y;xR
	char buf[128];
	char * p = buf;

	int c;
	int l = 0;
	while ((c = fgetc(stdin)) != 'R' && l + 1 < 128) {
		*p++ = c;
		l++;
	}
	*p = '\0';

	treset();

	// first char is ESC, second is [
	char * y = buf + 2;
	char * x = strchr(buf, ';');
	if (!x) {
		yx[0] = -1;
		yx[1] = -1;
		return;
	}

	*x = '\0';
	x++;

	yx[0] = strtod(y, NULL);
	yx[1] = strtod(x, NULL);
}


void print_prompt(void) {
	int yx[2];
	getyx(yx);

	if (yx[1] != 1) {
		// print a highlighted %
		// and skip to next line
		puts("\033[7m%\033[0m");
	}

	fputs("> ", stdout);
}


void clrline(void) {
	fputs("\33[2K\r", stdout);
}
