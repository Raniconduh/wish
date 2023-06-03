#include <stdlib.h>
#include <string.h>

#include "var.h"
#include "hashmap.h"


extern char ** environ;
static hashmap * varmap;


static var * new_var(void) {
	var * v = malloc(sizeof(var));
	v->var = NULL;
	v->val = NULL;
	v->env = false;
	return v;
}


static void destroyer(void * a) {
	var * v = (var*)a;
	free(v->var);
	free(v->val);
	free(v);
}


void var_parse(const char * s, char * vpair[2]) {
	char * eq = strchr(s, '=');
	if (!eq) {
		vpair[0] = strdup(s);
		vpair[1] = strdup("");
		return;
	}

	vpair[1] = strdup(eq + 1);
	vpair[0] = strndup(s, eq - s);
}


void var_set(char * name, char * value, bool env) {
	var * v = new_var();
	v->var = name;
	v->val = value;
	v->env = env;
	hashmap_insert(varmap, name, (void*)v);
	if (env) setenv(name, value, true);
}


void var_init(void) {
	varmap = new_hashmap(destroyer);

	// insert environment variables into map
	for (char ** p = environ; *p; p++) {
		char * vpair[2];
		var_parse(*p, vpair);
		var_set(V_NAME(vpair), V_VAL(vpair), true);
	}
}


void var_destroy(void) {
	destroy_hashmap(varmap);
}


var * var_get(char * vname) {
	return (var*)hashmap_get(varmap, vname);
}


char * var_expand(char * vname) {
	var * v = var_get(vname);
	if (!v) return NULL;
	return v->val;
}


void var_remove(char * name) {
	hashmap_remove(varmap, name);
}


var * var_walk(void) {
	return (var*)hashmap_walk(varmap);
}
