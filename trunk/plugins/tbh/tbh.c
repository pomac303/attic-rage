/* generates inane phrases based on over-used acronyms on Quakenet */
/* propz to sid, folks at demon internet (thus!), peeps at mathengine */
/* bart@bart666.com */

/* compile with gcc: gcc -O2 -o tbh tbh.c */
/* history :
 * v1.0 (26-01-03)
 * v2.0 (27-01-03): added more words, seperated smilies, added capitals
 * v2.1 (27-01-03): more words and smilies
 * v2.2 (28-01-03): missed a NULL in strupper(), repeat catcher was NULL
 * in every loop
 * v2.3 (21-12-04): now ported to rage
 *
 *  Rage plugin version by Ian Kumlien <pomac@vapor.com>
 *
 */	

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include "rage-plugin.h"

static rage_plugin *ph;
static int initialized = 0;
static rage_hook *hook_handle;
static char *tbh_buf;

/* add more words here */
static char *words[] =
{
	"tbh", /* to be honest */
	"fs",  /* fucks sake */
	"omfg", /* oh my fucking god */
	"stfu", /* shut the fuck up */
	"ffs", /* for fucks sake */
	"wtf", /* what the fuck */
	"omg", /* oh my god */
	"tbfh", /* to be fucking honest */
	"nfw", /* no fucking way */
	"plz", /* please */
	"n00b", /* newbie */
	"hax", /* hacks */
	"lol", /* laugh out loud */
	NULL
};
static char *smilies[] =
{
	":D", ":P", ":/", ":)", NULL
};	

#define MAX_WORDS 9
#define BUF_SIZE  512
#define SMILEY_RATIO 3 /* 1:SMILEY_RATIO chance of appearing */
#define CAPS_RATIO 7

/* this is a really poor strupper() but omg wtf stfu n00b */
char *strupper(const char *str)
{
	static char *buf = NULL;
	int i, len = strlen(str);

	if(buf) free(buf);
	buf = calloc(len + 1, 1);
	for(i = 0; i < len; i++)
		buf[i] = toupper(str[i]);

	return buf;
}

int tbh(int parc, char *parv[], void *userdata)
{
	char *buf = userdata, *last = NULL;
	int num_words, num_smilies, length, charlength, i;

	/* count words */
	for(num_words = 0; words[num_words] != NULL; num_words++);
	for(num_smilies = 0; smilies[num_smilies] != NULL; num_smilies++);
	if(!num_words)
		return 1;

	buf[0] = 0; /* move to the beginning of the buffer */

	/* generate phrase */
	srand(time(NULL));

	length = rand() % MAX_WORDS + 1;
	for(i = 0, charlength = 0; i < length; i++)
	{
		char *w;

		w = words[rand() % num_words];
		if(!w)
			continue;
		if(w == last)
			continue; /* don't use the same word as before */
		if(strlen(w) + charlength > BUF_SIZE)
			break;
		
		/* add word, randomly capitalising */
		sprintf(buf, "%s%s ", buf, !(rand() % CAPS_RATIO) ? strupper(w) : w);
		charlength += strlen(w) + 1; /* space */
		last = w;
	}

	/* add a smiley? */
	if(num_smilies && !(rand() % SMILEY_RATIO))
		sprintf(buf, "%s%s", buf, smilies[rand() % num_smilies]);
	
	/* dump phrase */
	rage_commandf(ph, "MSG %s %s", rage_get_info(ph, "channel"), buf);

	return 1;
}

int rage_plugin_init (rage_plugin *plugin_handle,
		char **plugin_name,
		char **plugin_desc,
		char **plugin_version,
		char *arg)
{
	if (initialized != 0)
	{
		rage_print(plugin_handle, "«Error»\ttbh.c already loaded.\n");
		return 0;
	}
	if ((tbh_buf = calloc(BUF_SIZE, 1)))
		initialized = 1;
	else
		return 0;

	ph = plugin_handle;

	*plugin_name = "tbh";
	*plugin_desc = "The essential annoyance!";
	*plugin_version = "2.3";

	rage_print(plugin_handle, "«tbh»\ttbh.c by Bart King <bart@bart666.com>. Ported to Rage by Ian Kumlien <pomac@vapor.com>\n");

	hook_handle = rage_hook_command(ph, "TBH", RAGE_PRI_NORM, tbh, "/TBH", tbh_buf);

	return 1;
}

int rage_plugin_deinit (void)
{
	rage_set_context(ph, rage_find_context(ph, NULL, NULL));
	rage_print(ph, "«tbh»\tBeing removed...\n");
	rage_unhook(ph, hook_handle);
	free(tbh_buf);
	return 1;
}
