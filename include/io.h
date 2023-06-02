#ifndef IO_H
#define IO_H

#include "parse.h"


int redirect_input(cmd *);
int redirect_output(cmd*);
void restore_input(void);
void restore_output(void);
void restore_io(void);
void pipe_input(cmd *);
void pipe_output(cmd *);
void print_prompt(void);
void clrline(void);
void tsetraw(void);
void treset(void);
void getyx(int [2]);

#endif /* IO_H */
