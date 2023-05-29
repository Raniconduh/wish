#ifndef HISTORY_H
#define HISTORY_H


typedef struct _He {
	struct _He * next;
	struct _He * prev;

	char * dat;
} he;


extern struct _History {
	he * hist;
	he * last;

	int entries;
} history;


void history_add(char *);
char * history_get(int);

#endif /* HISTORY_H */
