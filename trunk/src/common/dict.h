/*
 * This is a wrapper for old dict behavior, it simplifies things and leaves
 * the new code needed out of the main code section.
 * 
 * Copyright Ian Kumlien <pomac@vapor.com>
 * 
 * Distributed under the GNU GPL
 */

#include "set.h"

#if !defined(DICT_H)
#define DICT_H

typedef struct set *dict_t;
typedef struct set_node *dict_iterator_t;

#define dict_first(DICT) (set_first(DICT))
void* iter_data(struct set_node *node);
#define iter_next(ITER) ((ITER)->next)
#define iner_prev(ITER) ((ITER)->prev)
char * iter_key(struct set_node *node);

dict_t dict_new(void);
dict_t dict_numeric_new(void);

void dict_005_insert(dict_t dict, const char *key, void *data);
void dict_cmd_insert(dict_t dict, const char *key, void *data);
void dict_capab_insert(dict_t dict, const char *key);
void dict_numeric_insert(dict_t dict, const int *numeric, void *data);
#define dict_size(DICT) ((SET)->count)
void* dict_find(dict_t dict, const char *key, int *present);
void* dict_numeric_find(dict_t dict, const int *numeric, int *present);
void dict_remove(dict_t dict, char *key);
#define dict_delete(SET) (set_clear(SET))

#endif /* !defined(DICT_H) */
