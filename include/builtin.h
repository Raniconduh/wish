#ifndef BUILTIN_H
#define BUILTIN_H

#include <stdbool.h>

#include "parse.h"

typedef int(*bi_fn)(arg*);


typedef struct {
	char * name;
	bi_fn fn;
	bool forkme;
} builtin;


builtin * check_builtin(const char *);

#endif /* BUILTIN_H */
