/*
** debug.c
** Debug tracking code, for memory tracking, etc.
**
** (c) 2004 The Rage Development Team
*/

/* TODO: be a lot cleaner when memory allocations fail. */

#include "rage.h"

#if !defined(NDEBUG) || defined(USE_DEBUG)

/* Force undefination of defined stuff in context to this file */
#undef malloc
#undef free
#undef strdup
#undef realloc

static int current_mem_usage;

static struct mem_block
{
	char *file;
	void *buf;
	int size;
	int line;
	int total;
	struct mem_block *next;
} *mroot = NULL;
#endif

void *DBUG_malloc(int size, char *file, int line)
{
#if !defined(NDEBUG) || defined(USE_DEBUG)
	void *ret;
	struct mem_block *block;

	current_mem_usage += size;
	ret = malloc(size);
	if(!ret)
	{
		printf("Out of memory! (%d)\n", current_mem_usage);
		exit(255); // FIXME: cannot use exit()!
	}

	block = malloc(sizeof(struct mem_block));
	block->buf = ret;
	block->size = size;
	block->next = mroot;
	block->line = line;
	block->file = strdup(file);
	mroot = block;

	fprintf(stderr, "%s:%d malloc(%d) (total %d)\n", file, line, size, current_mem_usage);
	return ret;
#else
	return NULL; /* Won't get called, so don't panic */
#endif
}

void *DBUG_realloc(char *old, int len, char *file, int line)
{
#if !defined(NDEBUG) || defined(USE_DEBUG)
	char *ret;

	ret = DBUG_malloc(len, file, line);
	if(ret)
	{
		strcpy(ret, old);
		DBUG_free(old, file, line);
	}
	return ret;
#else
	return NULL;
#endif
}

void *DBUG_strdup(char *str, char *file, int line)
{
#if !defined(NDEBUG) || defined(USE_DEBUG)
	void *ret;
	int size;

	size = (int)strlen (str) + 1;
	fprintf(stderr, "DBUG_strdup: '%s'\n", str);
	ret = DBUG_malloc(size, file, line);

	return ret;
#else
	return NULL;
#endif
}

void DBUG_DumpMemoryList(void)
{
#if !defined(NDEBUG) || defined(USE_DEBUG)
	struct mem_block *cur, *p;
	GSList *totals = 0;
	GSList *list;

	cur = mroot;
	while (cur)
	{
		list = totals;
		while (list)
		{
			p = list->data;
			if (p->line == cur->line && strcmp (p->file, cur->file) == 0)
			{
				p->total += p->size;
				break;
			}
			list = list->next;
		}
		if (!list)
		{
			cur->total = cur->size;
			totals = g_slist_prepend (totals, cur);
		}
		cur = cur->next;
	}

	fprintf(stderr, "file              line   size    num  total\n");  
	list = totals;
	while (list)
	{
		cur = list->data;
		fprintf (stderr, "%-15.15s %6d %6d %6d %6d\n", cur->file, cur->line,
					cur->size, cur->total/cur->size, cur->total);
		list = list->next;
	}
#endif
}

void DBUG_free(void *buf, char *file, int line)
{
#if !defined(NDEBUG) || defined(USE_DEBUG)
	struct mem_block *cur, *last;

	if (buf == NULL)
	{
		fprintf(stderr, "%s:%d DBUG_free: attempt to free NULL\n", file, line);
		return;
	}

	last = NULL;
	cur = mroot;
	while (cur)
	{
		if (buf == cur->buf)
			break;
		last = cur;
		cur = cur->next;
	}
	if (cur == NULL)
	{
		printf ("%s:%d DBUG_free: tried to free unknown block %lx!\n",
				  file, line, (unsigned long)(buf));
		free(buf);
		return;
	}
	current_mem_usage -= cur->size;
	printf ("%s:%d DBUG_free: %d bytes, %d usage\n",
				file, line, cur->size, current_mem_usage);
	if (last)
		last->next = cur->next;
	else
		mroot = cur->next;
	free (cur->file);
	free (cur);
#endif
}
