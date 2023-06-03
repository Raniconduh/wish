#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdint.h>


// collision list
typedef struct col_list {
	struct col_list * next;
	struct col_list * prev;

	unsigned long hash;
	char * key;
	void * val;
} col_list;


typedef struct {
	col_list * entries;
	col_list * last;
} bucket;


typedef struct {
	void (*destroyer)(void*);
	bucket ** buckets;
	size_t len; // total entries
	size_t max;
} hashmap;


hashmap * new_hashmap(void (*destroyer)(void*));
void hashmap_insert(hashmap *, char *, void *);
void * hashmap_get(hashmap *, char *);
void hashmap_remove(hashmap *, char *);
void destroy_hashmap(hashmap *);
void * hashmap_walk(hashmap *);

#endif /* HASHMAP_H */
