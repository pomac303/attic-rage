/*
 * This is a wrapper for old dict behavior, it simplifies things and leaves
 * the new code needed out of the main code section.
 *
 * Copyright Ian Kumlien <pomac@vapor.com>
 *
 * Distributed under the GNU GPL
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "set.h"

struct rage_dict_data {
	char *key;
	void *value;
};

int
dict_cmp(const void *a, const void *b)
{
	return strcasecmp(*(const char **)a, *(const char **)b);
}

struct set *
dict_new(void)
{
	return set_alloc((set_compare_f *)&dict_cmp, NULL);
}

/* dict_005_insert, 005 data, the data is written directly
 * to the node, no free is needed */
void
dict_005_insert(struct set *set, char *key, char *value)
{
	struct rage_dict_data *data;
	struct set_node *node;

	if (!key)
		return;
	
	data = set_find(set, &key);
	
	/* remove the old node if there is one */
	if(data)
		set_remove(set, &key, 0);
	
	/* create a new node (value might be null) */
	node = set_node_alloc(sizeof(*data) + strlen(key) + 
			(value ? strlen(value) : 0) +2);
	data = set_node_data(node);
	data->key = strcpy((char*)(data + 1), key);
	/* (value might be null) */
	data->value = value ? strcpy(data->key + strlen(key) +1, value) : NULL;

	set_insert(set, node);
}

/* dict_capab_insert, insert capab data. The data is
 * written directly to the node, no free is needed. */
void
dict_capab_insert(struct set *set, char *key)
{
	struct rage_dict_data *data;
	struct set_node *node;
	char tmp[50], *ptmp;

	if (!key)
		return;

	strcpy(tmp, "CAPAB-");
	strncpy(tmp + 6, key, sizeof(tmp) - 6);

	/* important, set_find needs (char **) */
	ptmp = tmp;
	data = set_find(set, &ptmp);

	/* already present, ignore */
	if (data)
		return;

	node = set_node_alloc(sizeof(*data) + strlen(tmp) +1);
	data = set_node_data(node);
	data->key = strcpy((char*)(data + 1), tmp);
	data->value = NULL;

	set_insert(set, node);
}

/* dict_cmd_insert, insert data with 2 pointers since
 * we don't need to free it or not. */
void
dict_cmd_insert(struct set *set, char *key, char *value)
{
	struct rage_dict_data *data;
	struct set_node *node;

	if (!key)
		return;
	
	data = set_find(set, &key);

	/* reuse old data if we have some */
	if(data)
	{
		if(data->value)
			free(data->value);
		data->value = value;
	}
	else
	{
		/* create a new node */
		node = set_node_alloc(sizeof(*data));
		data = set_node_data(node);
		data->key = key;
		data->value = value;

		set_insert(set, node);
	}
}

/* dict_find, find the data, return the value and
 * set found according to state */
void *
dict_find(struct set *set, char *key, int *found)
{
	struct rage_dict_data *data = set_find(set, &key);
	
	if (data)
	{
		*found = 1;
		return data->value;
	}
	else
	{
		/* make sure that found is cleared */
		*found = 0;
		return NULL;
	}
}

/* iter_key gets the key data from the node */
char *
iter_key(struct set_node *node)
{
	struct rage_dict_data *data = set_node_data(node);
	/* should always be ok since we always set key */
	return data->key;
}

/* iter_data get the value from the node */
void *
iter_data(struct set_node *node)
{
	struct rage_dict_data *data = set_node_data(node);
	/* should always be ok since value is either NULL or valid */
	return data->value;
}

