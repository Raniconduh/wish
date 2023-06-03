#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "hashmap.h"


#define HASHMAP_RESIZE_LEN 128
#define HASHMAP_RESIZE_FACT 0.75


// djb2 hash
static unsigned long hashs(const char * s) {
	unsigned long hash = 5381;
	int c;
	while ((c = *s++)) {
		hash = (hash << 5) + hash + c;
	}
	return hash;
}


static bucket * new_bucket(void) {
	bucket * b = malloc(sizeof(bucket));
	b->entries = NULL;
	b->last = NULL;
	return b;
}


static col_list * new_col_list(void) {
	col_list * cl = malloc(sizeof(col_list));
	cl->next = NULL;
	cl->prev = NULL;
	cl->hash = 0;
	cl->key = NULL;
	cl->val = NULL;
	return cl;
}


static void destroy_col(col_list * cl) {
	free(cl->key);
	free(cl);
}


static col_list * append_col(bucket * b) {
	if (!b->entries) {
		b->entries = new_col_list();
		b->last = b->entries;
		return b->entries;
	}

	b->last->next = new_col_list();
	b->last->next->prev = b->last;
	b->last = b->last->next;
	return b->last;
}


static bucket * hashmap_get_bucket(hashmap * hm, char * key) {
	unsigned long hash = hashs(key);
	size_t index = hash % hm->max;
	return hm->buckets[index];
}


static col_list * search_col_list(bucket * b, char * key) {
	for (col_list * cl = b->entries; cl; cl = cl->next) {
		if (!strcmp(key, cl->key)) return cl;
	}
	return NULL;
}


hashmap * new_hashmap(void (*destroyer)(void*)) {
	hashmap * hm = malloc(sizeof(hashmap));
	hm->destroyer = destroyer;
	hm->buckets = malloc(sizeof(bucket) * HASHMAP_RESIZE_LEN);
	hm->len = 0;
	hm->max = HASHMAP_RESIZE_LEN;

	for (size_t i = 0; i < hm->max; i++) {
		hm->buckets[i] = NULL;
	}

	return hm;
}


static void hashmap_resize(hashmap * hm) {
	size_t nmax = hm->max + HASHMAP_RESIZE_LEN;
	bucket ** nb = malloc(sizeof(bucket) * nmax);

	for (size_t i = 0; i < nmax; i++) {
		nb[i] = NULL;
	}

	for (size_t i = 0; i < hm->max; i++) {
		bucket * ob = hm->buckets[i];
		if (!ob || !ob->entries) continue;

		for (col_list * cl = ob->entries; cl;) {
			col_list * next = cl->next;

			size_t nindex = cl->hash % nmax;
			if (!nb[nindex]) {
				nb[nindex] = new_bucket();
				nb[nindex]->entries = cl;
				nb[nindex]->last = cl;
				cl->prev = NULL;
				cl->next = NULL;
			} else {
				col_list * last = nb[nindex]->last;
				last->next = cl;
				cl->next = NULL;
				cl->prev = last;
				nb[nindex]->last = cl;
			}

			cl = next;
		}

		free(ob);
	}

	free(hm->buckets);
	hm->buckets = nb;
	hm->max = nmax;
}


void hashmap_insert(hashmap * hm, char * key, void * val) {
	if ((float)hm->len / (float)hm->max >= HASHMAP_RESIZE_FACT) {
		hashmap_resize(hm);
	}

	col_list * cl = NULL;

	unsigned long hash = hashs(key);
	size_t index = hash % hm->max;
	if (!hm->buckets[index]) {
		hm->buckets[index] = new_bucket();
	} else {
		for (col_list * l = hm->buckets[index]->entries; l; l = l->next) {
			// overrite the old collision entry
			if (!strcmp(l->key, key)) {
				cl = l;
				hm->destroyer(cl->val);
				cl->val = val;
			}
		}
	}

	if (!cl) {
		cl = append_col(hm->buckets[index]);
		cl->hash = hash;
		cl->key = strdup(key);
		cl->val = val;

		hm->len++;
	}
}


void * hashmap_get(hashmap * hm, char * key) {
	bucket * b = hashmap_get_bucket(hm, key);
	if (!b) return NULL;
	col_list * cl = search_col_list(b, key);
	if (!cl) return NULL;
	return cl->val;
}


void hashmap_remove(hashmap * hm, char * key) {
	bucket * b = hashmap_get_bucket(hm, key);
	if (!b) return;

	col_list * cl = search_col_list(b, key);
	if (!cl) return;

	col_list * prev = cl->prev;
	col_list * next = cl->next;

	if (!prev) {
		b->entries = next;
	} else {
		prev->next = next;
		next->prev = prev;
	}

	hm->destroyer(cl->val);
	destroy_col(cl);
}


void * hashmap_walk(hashmap * hm) {
	static size_t curbuck = 0;
	static col_list * col = NULL;

	for (size_t i = curbuck; i < hm->max; i++) {
		bucket * b = hm->buckets[i];
		if (!b || !b->entries) continue;

		if (!col) col = b->entries;
		for (col_list * cl = col; cl; cl = cl->next) {
			col = cl->next;
			curbuck = i;
			if (!col) curbuck++;

			return cl->val;
		}
		col = NULL;
	}

	curbuck = 0;
	col = NULL;

	return NULL;
}


void destroy_hashmap(hashmap * hm) {
	for (size_t i = 0; i < hm->max; i++) {
		bucket * b = hm->buckets[i];
		if (!b) continue;

		// free each collision in the bucket
		for (col_list * cl = b->entries; cl;) {
			col_list * next = cl->next;
			hm->destroyer(cl->val);
			destroy_col(cl);
			cl = next;
		}

		free(b);
	}

	free(hm->buckets);
	free(hm);
}
