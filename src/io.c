#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stddef.h>

#include "io.h"


static int stdin_back = -1;
static int stdout_back = -1;


int redirect_input(cmd * com) {
	if (!com->redir_in) return 0;

	int infd = open(com->redir_in, O_RDONLY);
	if (infd == -1) return -1;
	stdin_back = dup(STDIN_FILENO);
	dup2(infd, STDIN_FILENO);

	return 0;
}


int redirect_output(cmd * com) {
	if (!com->redir_out) return 0;

	int outfd = open(com->redir_out, O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (outfd == -1) return -1;
	stdout_back = dup(STDOUT_FILENO);
	dup2(outfd, STDOUT_FILENO);

	return 0;
}


void restore_io(void) {
	if (stdin_back > -1) {
		close(STDIN_FILENO);
		dup2(stdin_back, STDIN_FILENO);

		stdin_back = -1;
	}

	if (stdout_back > -1) {
		close(STDOUT_FILENO);
		dup2(stdout_back, STDOUT_FILENO);

		stdout_back = -1;
	}
}


void print_prompt(void) {
	fputs("> ", stdout);
}


void clrline(void) {
	fputs("\33[2K\r", stdout);
}
