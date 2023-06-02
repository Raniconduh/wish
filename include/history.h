#ifndef HISTORY_H
#define HISTORY_H


typedef struct he {
	struct he * next;
	struct he * prev;

	char * dat;
} he;


extern struct history {
	he * hist;
	he * last;

	int entries;
} history;


void history_add(char *);
char * history_get(int);

#endif /* HISTORY_H */
