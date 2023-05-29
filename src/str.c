#include <stdio.h>
#include <stdlib.h>

#include "str.h"


str * new_str(void) {
	str * s = malloc(sizeof(str));
	s->f = open_memstream(&s->p, &s->s);
	return s;
}


void str_addc(str * s, int c) {
	fputc(c, s->f);
}


size_t str_len(str * s) {
	fflush(s->f);
	return s->s;
}


char * str_conv(str * s) {
	fclose(s->f);
	char * p = s->p;
	free(s);
	return p;
}


void str_destroy(str * s) {
	char * p = str_conv(s);
	free(p);
}
