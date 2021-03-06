#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "rage.h"
#include "rage-plugin.h"

#ifdef WIN32
#define strcasecmp stricmp
#endif

static rage_plugin *ph;	/* plugin handle */
static GSList *timer_list = NULL;

#define STATIC
#define HELP \
"Usage: TIMER [-refnum <num>] [-repeat <num>] <seconds> <command>\n" \
"       TIMER [-quiet] -delete <num>"

typedef struct
{
	rage_hook *hook;
	rage_context *context;
	char *command;
	int ref;
	int timeout;
	int repeat;
	unsigned int forever:1;
} timer;

static void
timer_del (timer *tim)
{
	timer_list = g_slist_remove (timer_list, tim);
	free (tim->command);
	rage_unhook (ph, tim->hook);
	free (tim);
}

static void
timer_del_ref (int ref, int quiet)
{
	GSList *list;
	timer *tim;

	list = timer_list;
	while (list)
	{
		tim = list->data;
		if (tim->ref == ref)
		{
			timer_del (tim);
			if (!quiet)
				rage_printf (ph, "Timer %d deleted.\n", ref);
			return;
		}
		list = list->next;
	}
	if (!quiet)
		rage_print (ph, "No such ref number found.\n");
}

static int
timeout_cb (timer *tim)
{
	if (rage_set_context (ph, tim->context))
	{
		rage_command (ph, tim->command);

		if (tim->forever)
			return 1;

		tim->repeat--;
		if (tim->repeat > 0)
			return 1;
	}

	timer_del (tim);
	return 0;
}

static void
timer_add (int ref, int timeout, int repeat, char *command)
{
	timer *tim;
	GSList *list;

	if (ref == 0)
	{
		ref = 1;
		list = timer_list;
		while (list)
		{
			tim = list->data;
			if (tim->ref >= ref)
				ref = tim->ref + 1;
			list = list->next;
		}
	}

	tim = malloc (sizeof (timer));
	tim->ref = ref;
	tim->repeat = repeat;
	tim->timeout = timeout;
	tim->command = strdup (command);
	tim->context = rage_get_context (ph);
	tim->forever = FALSE;

	if (repeat == 0)
		tim->forever = TRUE;

	tim->hook = rage_hook_timer (ph, timeout * 1000, (void *)timeout_cb, tim);
	timer_list = g_slist_append (timer_list, tim);
}

static void
timer_showlist (void)
{
	GSList *list;
	timer *tim;

	if (timer_list == NULL)
	{
		rage_print (ph, "No timers installed.\n");
		return;
	}

	rage_print (ph, "Ref   T   R Command\n");
	list = timer_list;
	while (list)
	{
		tim = list->data;
		rage_printf (ph, "%3d %3d %3d %s\n", tim->ref, tim->timeout,
						  tim->repeat, tim->command);
		list = list->next;
	}
}

static int
timer_cb (int parc, char *parv[], void *userdata)
{
	int repeat = 1;
	int timeout;
	int offset = 0;
	int ref = 0;
	int quiet = FALSE;
	char *command;

	if (!parv[1][0])
	{
		timer_showlist ();
		return RAGE_EAT_RAGE;
	}

	if (strcasecmp (parv[1], "-quiet") == 0)
	{
		quiet = TRUE;
		offset++;
	}

	if (strcasecmp (parv[1 + offset], "-delete") == 0)
	{
		timer_del_ref (atoi (parv[2 + offset]), quiet);
		return RAGE_EAT_RAGE;
	}

	if (strcasecmp (parv[1 + offset], "-refnum") == 0)
	{
		ref = atoi (parv[3 + offset]);
		offset += 2;
	}

	if (strcasecmp (parv[1 + offset], "-repeat") == 0)
	{
		repeat = atoi (parv[2 + offset]);
		offset += 2;
	}

	timeout = atoi (parv[1 + offset]);
	command = parv[2 + offset];

	if (timeout < 1 || !command[0])
		rage_print (ph, HELP);
	else
		timer_add (ref, timeout, repeat, command);

	return RAGE_EAT_RAGE;
}

int
#ifdef STATIC
timer_plugin_init
#else
rage_plugin_init
#endif
				(rage_plugin *plugin_handle, char **plugin_name,
				char **plugin_desc, char **plugin_version, char *arg)
{
	/* we need to save this for use with any rage_* functions */
	ph = plugin_handle;

	*plugin_name = "Timer";
	*plugin_desc = "IrcII style /TIMER command";
	*plugin_version = "";

	rage_hook_command (ph, "TIMER", RAGE_PRI_NORM, timer_cb, HELP, 0);

	return 1;       /* return 1 for success */
}
