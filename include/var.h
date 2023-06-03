#ifndef VAR_H
#define VAR_H

#include <stdbool.h>

#include "hashmap.h"


#define V_NAME(p) (p[0])
#define V_VAL(p) (p[1])


typedef struct {
	char * var;
	char * val;
	bool env;
} var;


void var_init(void);
char * var_expand(char *);
void var_destroy(void);
void var_remove(char *);
var * var_walk(void);
void var_parse(const char *, char * [2]);
void var_set(char *, char *, bool);
var * var_get(char *);


#endif /* VAR_H */
