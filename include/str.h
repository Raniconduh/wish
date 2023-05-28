#ifndef STR_H
#define STR_H

typedef struct {
	char * p;
	size_t s;
	FILE * f;
} str;

str * new_str(void);
void str_addc(str *, int);
size_t str_len(str *);
char * str_conv(str *);
void str_destroy(str *);

#endif /* STR_H */
