/* set.c - Splay tree set
 * Copyright 2004 Michael Poole
 * arch-tag: 277a1956-124b-4dee-91d4-514ca6edc618
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <set.h>
#include <assert.h>

struct set *
set_alloc(set_compare_f *compare, set_cleanup_f *cleanup)
{
    struct set *set;

    assert(compare != NULL);
    set = malloc(sizeof(*set));
    set->compare = compare;
    set->cleanup = cleanup;
    set->root = NULL;
    set->count = 0;
    return set;
}

struct set_node *
set_first(struct set *set)
{
    struct set_node *node = set->root;
    if (node)
        while (node->l)
            node = node->l;
    return node;
}

static int
set_splay(struct set *set, const void *datum)
{
    struct set_node N, *node, *l, *r;
    int res;

    if (!set->root)
        return 0;
    node = set->root;
    N.l = N.r = NULL;
    l = r = &N;

    while (1) {
        res = set->compare(datum, set_node_data(node));
        if (!res)
            break;
        if (res < 0) {
            if (!node->l)
                break;
            res = set->compare(datum, set_node_data(node->l));
            if (res < 0) {
                struct set_node *y = node->l;
                node->l = y->r;
                y->r = node;
                node = y;
                if (!node->l)
                    break;
            }
            r->l = node;
            r = node;
            node = node->l;
        } else { /* res > 0 */
            if (!node->r)
                break;
            res = set->compare(datum, set_node_data(node->r));
            if (res > 0) {
                struct set_node *y = node->r;
                node->r = y->l;
                y->l = node;
                node = y;
                if (!node->r)
                    break;
            }
            l->r = node;
            l = node;
            node = node->r;
        }
    }
    l->r = node->l;
    r->l = node->r;
    node->l = N.r;
    node->r = N.l;
    set->root = node;
    return res;
}

static void
set_dispose_node(struct set *set, struct set_node *node)
{
    if (set->cleanup)
        set->cleanup(set_node_data(node));
    free(node);
}

void
set_insert(struct set *set, struct set_node *node)
{
    if (set->root) {
        int res = set_splay(set, set_node_data(node));
        if (res < 0) {
            node->l = set->root->l;
            node->r = set->root;
            set->root->l = NULL;
            node->prev = set->root->prev;
            node->next = set->root;
        } else if (res > 0) {
            node->l = set->root;
            node->r = set->root->r;
            set->root->r = NULL;
            node->prev = set->root;
            node->next = set->root->next;
        } else {
            assert(node != set->root && "attempted to reinsert node into set");
            memcpy(node, set->root, sizeof(*node));
            set_dispose_node(set, set->root);
            set->count--;
        }
        if (node->prev)
            node->prev->next = node;
        if (node->next)
            node->next->prev = node;
    } else {
        node->l = node->r = node->next = node->prev = NULL;
    }
    set->root = node;
    set->count++;
}

void *
set_find(struct set *set, const void *datum)
{
    if (!set || !set->root || !datum)
        return NULL;
    if (set_splay(set, datum))
        return NULL;
    return set_node_data(set->root);
}

struct set_node *
set_lower(struct set *set, const void *datum)
{
    int res;
    if (!set || !set->root || !datum)
        return NULL;
    res = set_splay(set, datum);
    if (res > 0)
        return set->root->next;
    else
        return set->root;
}

int
set_remove(struct set *set, void *datum, int no_dispose)
{
    struct set_node *new_root, *old_root;

    if (!set || !set->root /*|| set_splay(set, datum) */)
        return 0;

    old_root = set->root;
    if (!set->root->l) {
        new_root = set->root->r;
    } else {
        set->root = set->root->l;
        set_splay(set, datum);
        new_root = set->root;
        new_root->r = old_root->r;
        set->root = old_root;
    }
    if (set->root->prev)
        set->root->prev->next = set->root->next;
    if (set->root->next)
        set->root->next->prev = set->root->prev;
    set->root = new_root;
    set->count--;
    if (!no_dispose)
        set_dispose_node(set, old_root);
    return 1;
}

void
set_clear(struct set *set)
{
    struct set_node *it, *next;
    unsigned int ii;

    if (!set)
        return;
    for (it = set_first(set), ii = 0; it; it = next, ++ii) {
        next = set_next(it);
        set_dispose_node(set, it);
    }
    assert(ii == set->count);
    set->root = NULL;
    set->count = 0;
}

int
set_compare_charp(const void *a_, const void *b_)
{
    char * const *a = a_, * const *b = b_;
    return strcasecmp(*a, *b);
}

int
set_compare_int(const void *a_, const void *b_)
{
    const int *a = a_, *b = b_;
    return *b - *a;
}
