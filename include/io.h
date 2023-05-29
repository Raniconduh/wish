#ifndef IO_H
#define IO_H

#include "parse.h"


int redirect_input(cmd *);
int redirect_output(cmd*);
void restore_io(void);
void print_prompt(void);
void clrline(void);

#endif /* IO_H */
