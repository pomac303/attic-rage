/* set.h - Splay tree set
 * Copyright 2004 Michael Poole
 * arch-tag: 416c49bb-899b-4b3b-b968-3aecc8292dda
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */

#if !defined(SRVX_SET_H)
#define SRVX_SET_H

#define xmalloc(SIZE)   (g_malloc(SIZE))
#define xfree(PTR)      (g_free(PTR))

/** Element destruction function.  It must dereference any memory
 * pointed to by the data element pointed to, but must not free the
 * element itself.
 */
typedef void set_cleanup_f(void*);

/** Set comparison function type.  It works like the function passed
 * to qsort(), and must return a number less than, equal to or greater
 * than zero if the first argument is less than, equal to or greater
 * (respectively) than the second argument.  The second argument is
 * always an element of the set; the first argument may or may not be
 * in the set.
 */
typedef int set_compare_f(const void*, const void*);

struct set_node
{
    struct set_node *l, *r, *prev, *next;
};

struct set
{
    set_compare_f *compare;
    set_cleanup_f *cleanup;
    struct set_node *root;
    unsigned int count;
};

#define set_node(DATUM) (((struct set_node*)(DATUM))-1)
#define set_node_data(NODE) ((void*)((NODE)+1))
#define set_node_alloc(SIZE) ((struct set_node*)xmalloc(sizeof(struct set_node) + (SIZE)))
#define set_prev(NODE) ((NODE)->prev)
#define set_next(NODE) ((NODE)->next)

struct set *set_alloc(set_compare_f *compare, set_cleanup_f *cleanup);
#define set_size(SET) ((SET)->count)
void set_insert(struct set *set, struct set_node *node);
void *set_find(struct set *set, const void *datum);
struct set_node *set_first(struct set *set);
struct set_node *set_lower(struct set *set, const void *datum);
int set_remove(struct set *set, void *datum, int no_dispose);
void set_clear(struct set *set);

/* Functions you might use for set cleanup or compare. */
int set_compare_charp(const void *a_, const void *b_);
int set_compare_int(const void *a_, const void *b_);

#endif /* !defined(SRVX_SET_H) */
