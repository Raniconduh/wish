#include <stdlib.h>
#include <string.h>

#include "history.h"

#define MAX_HISTSIZE 64


struct history history = {
	NULL, NULL, 0	
};


static he * new_he(void) {
	he * h = malloc(sizeof(he));
	h->next = NULL;
	h->prev = NULL;
	h->dat = NULL;
	return h;
}


static void he_destroy(he * h) {
	free(h->dat);
	free(h);
}


void history_add(char * s) {
	if (!history.hist) {
		history.hist = new_he();
		history.last = history.hist;
	} else {
		he * tmp = history.hist;

		history.hist = new_he();
		history.hist->next = tmp;
		tmp->prev = history.hist;
	}

	history.hist->dat = strdup(s);
	history.entries++;

	if (history.entries == MAX_HISTSIZE) {
		he * tmp = history.last;
		history.last = history.last->prev;

		he_destroy(tmp);
		history.last->next = NULL;
		history.entries--;
	}
}


char * history_get(int n) {
	if (n >= history.entries || n < 0) return NULL;

	he * h = history.hist;
	for (int i = 0; i < n; i++) {
		h = h->next;		
	}

	return h->dat;
}
