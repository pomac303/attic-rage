/* Rage
 * Copyright (C) 2002 Peter Zelezny.
 *
 * Forked from the great work by Peter Zelezny on XChat.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/* Require plugin implementation header */
#include "rage.h"


#ifdef USE_GMODULE
#include <gmodule.h>
#else
#include <dlfcn.h>
#endif

#define DEBUG(x) {x;}

/* crafted to be an even 32 bytes */
struct _rage_hook
{
	rage_plugin *pl;	/* the plugin to which it belongs */
	char *name;			/* "xdcc" */
	void *callback;	/* pointer to xdcc_callback */
	char *help_text;	/* help_text for commands only */
	void *userdata;	/* passed to the callback */
	int tag;				/* for timers & FDs only */
	int type;			/* HOOK_* */
	int pri;	/* fd */	/* priority / fd for HOOK_FD only */
};

struct _rage_list
{
	int type;			/* LIST_* */
	GSList *pos;		/* current pos */
	GSList *next;		/* next pos */
	GSList *head;		/* for LIST_USERS only */
	struct notify_per_server *notifyps;	/* notify_per_server * */
};

enum
{
	LIST_CHANNELS,
	LIST_DCC,
	LIST_IGNORE,
	LIST_NOTIFY,
	LIST_USERS
};

enum
{
	HOOK_COMMAND,	/* /command */
	HOOK_SERVER,	/* PRIVMSG, NOTICE, numerics */
	HOOK_PRINT,	/* All print events */
	HOOK_TIMER,	/* timeouts */
	HOOK_FD,	/* sockets & fds */
	HOOK_DELETED	/* marked for deletion, ALWAYS LAST! */
};

dict_t plugin_list = NULL;	/* export for plugingui.c */
static GSList *hook_list[HOOK_DELETED];

extern const struct prefs vars[];	/* cfgfiles.c */

void
setup_plugin_commands(void)
{
	plugin_list = dict_new();
}

/* 31 bit string hash function */

static guint32
str_hash (const char *key)
{
	const char *p = key;
	guint32 h = *p;

	if (h)
		for (p += 1; *p != '\0'; p++)
			h = (h << 5) - h + *p;

	return h;
}

/* unload a plugin and remove it from our linked list */

static int
plugin_free (rage_plugin *pl, int do_deinit, int allow_refuse)
{
	GSList *list, *next;
	rage_hook *hook;
	rage_deinit_func *deinit_func;
	int i;

	/* fake plugin added by rage_plugingui_add() */
	if (pl->fake)
		goto xit;

	/* run the plugin's deinit routine, if any */
	if (do_deinit && pl->deinit_callback != NULL)
	{
		deinit_func = pl->deinit_callback;
		if (!deinit_func (pl) && allow_refuse)
			return FALSE;
	}

	/* remove all of this plugins hooks */
	for (i = 0; i < HOOK_DELETED; i++)
	{
		list = hook_list[i];
		while (list)
		{
			hook = list->data;
			next = list->next;
			if (hook->pl == pl)
				rage_unhook (NULL, hook);
			list = next;
		}
	}

#ifdef USE_PLUGIN
	if (pl->handle)
#ifdef USE_GMODULE
		g_module_close (pl->handle);
#else
		dlclose (pl->handle);
#endif
#endif

xit:
	/* remove the plugin */
	set_remove(plugin_list, pl, 0);

	if (pl->filename)
		free ((char *)pl->filename);
	free (pl);

#ifdef USE_PLUGIN
	fe_pluginlist_update ();
#endif

	return TRUE;
}

static rage_plugin *
plugin_list_add (rage_context *ctx, char *filename, const char *name,
		const char *desc, const char *version, void *handle,
		void *deinit_func, int fake)
{
	rage_plugin *pl;

	pl = malloc (sizeof (rage_plugin));
	pl->handle = handle;
	pl->filename = filename;
	pl->context = ctx;
	pl->name = (char *)name;
	pl->desc = (char *)desc;
	pl->version = (char *)version;
	pl->deinit_callback = deinit_func;
	pl->fake = fake;

	dict_cmd_insert(plugin_list, pl->name, pl);

	return pl;
}

static void *
rage_dummy (rage_plugin *ph)
{
	return NULL;
}

#ifdef WIN32
static int
rage_read_fd (rage_plugin *ph, GIOChannel *source, char *buf, int *len)
{
	return g_io_channel_read (source, buf, *len, len);
}
#endif

/* Load a static plugin */

void
plugin_add (rage_session *sess, char *filename, void *handle, void *init_func,
				void *deinit_func, char *arg, int fake)
{
	rage_plugin *pl;
	char *file;

	file = NULL;
	if (filename)
		file = strdup (filename);

	pl = plugin_list_add (sess, file, file, NULL, NULL, handle, deinit_func,
								 fake);

	if (!fake)
	{
		/* win32 uses these because it doesn't have --export-dynamic! */
		pl->rage_hook_command = rage_hook_command;
		pl->rage_hook_server = rage_hook_server;
		pl->rage_hook_print = rage_hook_print;
		pl->rage_hook_timer = rage_hook_timer;
		pl->rage_hook_fd = rage_hook_fd;
		pl->rage_unhook = rage_unhook;
		pl->rage_print = rage_print;
		pl->rage_printf = rage_printf;
		pl->rage_command = rage_command;
		pl->rage_commandf = rage_commandf;
		pl->rage_nickcmp = rage_nickcmp;
#ifdef PLUGIN_C /* FIXME: why? */
		pl->rage_set_context = rage_set_context;
		pl->rage_find_context = rage_find_context;
		pl->rage_get_context = rage_get_context;
#endif
		pl->rage_get_info = rage_get_info;
		pl->rage_get_prefs = rage_get_prefs;
		pl->rage_list_get = rage_list_get;
		pl->rage_list_free = rage_list_free;
		pl->rage_list_fields = rage_list_fields;
		pl->rage_list_str = rage_list_str;
		pl->rage_list_next = rage_list_next;
		pl->rage_list_int = rage_list_int;
		pl->rage_plugingui_add = rage_plugingui_add;
		pl->rage_plugingui_remove = rage_plugingui_remove;
		pl->rage_emit_print = rage_emit_print;
#ifdef WIN32
		pl->rage_read_fd = (void *) rage_read_fd;
#else
		pl->rage_read_fd = (void *) rage_dummy;
#endif
		pl->rage_list_time = rage_list_time;
		pl->rage_gettext = rage_gettext;
		pl->rage_send_modes = rage_send_modes;

		/* incase new plugins are loaded on older rage */
		pl->rage_dummy6 = rage_dummy;
		pl->rage_dummy5 = rage_dummy;
		pl->rage_dummy4 = rage_dummy;
		pl->rage_dummy3 = rage_dummy;
		pl->rage_dummy2 = rage_dummy;
		pl->rage_dummy1 = rage_dummy;

		/* run rage_plugin_init, if it returns 0, close the plugin */
		if (((rage_init_func *)init_func) (pl, &pl->name, &pl->desc, &pl->version, arg) == 0)
		{
			plugin_free (pl, FALSE, FALSE);
			return;
		}
	}

#ifdef USE_PLUGIN
	fe_pluginlist_update ();
#endif
}

/* kill any plugin by the given (file) name (used by /unload) */

int
plugin_kill (char *name, int by_filename)
{
	rage_plugin *pl;
	dict_iterator_t it;

	for (it=dict_first(plugin_list); it; it=iter_next(it))
	{
		pl = iter_data(it);
		/* static-plugins (plugin-timer.c) have a NULL filename */
		if ((by_filename && pl->filename && strcasecmp (name, pl->filename) == 0) ||
				(by_filename && pl->filename && 
				 strcasecmp (name, file_part (pl->filename)) == 0) ||
				(!by_filename && strcasecmp (name, pl->name) == 0))
		{
			/* statically linked plugins have a NULL filename */
			if (pl->filename != NULL && !pl->fake)
			{
				if (plugin_free (pl, TRUE, TRUE))
					return 1;
				return 2;
			}
		}
	}
	
	return 0;
}

/* kill all running plugins (at shutdown) */

void
plugin_kill_all (void)
{
	rage_plugin *pl;
	dict_iterator_t it;

	for (it=dict_first(plugin_list); it; it=iter_next(it))
	{
		pl = iter_data(it);
		if (!pl->fake)
			plugin_free (pl, TRUE, FALSE);
	}
}

#ifdef USE_PLUGIN

/* load a plugin from a filename. Returns: NULL-success or an error string */

char *
plugin_load (rage_session *sess, char *filename, char *arg)
{
	void *handle;
	rage_init_func *init_func;
	rage_deinit_func *deinit_func;

#ifdef USE_GMODULE
	/* load the plugin */
	handle = g_module_open (filename, 0);
	if (handle == NULL)
		return (char *)g_module_error ();

	/* find the init routine rage_plugin_init */
	if (!g_module_symbol (handle, "rage_plugin_init", (gpointer *)&init_func))
	{
		g_module_close (handle);
		return _("No rage_plugin_init symbol; is this really an rage plugin?");
	}

	/* find the plugin's deinit routine, if any */
	if (!g_module_symbol (handle, "rage_plugin_deinit", (gpointer *)&deinit_func))
		deinit_func = NULL;

#else
	char *error;
	char *filepart;

/* OpenBSD lacks this! */
#ifndef RTLD_GLOBAL
#define RTLD_GLOBAL 0
#endif

#ifndef RTLD_NOW
#define RTLD_NOW 0
#endif

	/* get the filename without path */
	filepart = file_part (filename);

	/* load the plugin */
	if (filepart &&
			/* xsys draws in libgtk-1.2, causing crashes, so force RTLD_LOCAL */
			(strstr (filepart, "local") || strncmp (filepart, "libxsys-1", 9) == 0))
		handle = dlopen (filename, RTLD_NOW);
	else
		handle = dlopen (filename, RTLD_GLOBAL | RTLD_NOW);
	if (handle == NULL)
		return (char *)dlerror ();
	dlerror ();		/* Clear any existing error */

	/* find the init routine rage_plugin_init */
	init_func = dlsym (handle, "rage_plugin_init");
	error = (char *)dlerror ();
	if (error != NULL)
	{
		dlclose (handle);
		return _("No rage_plugin_init symbol; is this really an rage plugin?");
	}

	/* find the plugin's deinit routine, if any */
	deinit_func = dlsym (handle, "rage_plugin_deinit");
	error = (char *)dlerror ();
#endif

	/* add it to our linked list */
	plugin_add (sess, filename, handle, init_func, deinit_func, arg, FALSE);

	return NULL;
}

static rage_session *ps;

static void
plugin_auto_load_cb (char *filename)
{
	char *pMsg;

	pMsg = plugin_load (ps, filename, NULL);
	if (pMsg)
	{
		PrintTextf (ps, "AutoLoad failed for: %s\n", filename);
		PrintText (ps, pMsg);
	}
}

void
plugin_auto_load (rage_session *sess)
{
	ps = sess;
#ifdef WIN32
	for_files ("./plugins", "*.dll", plugin_auto_load_cb);
	for_files (get_xdir_fs (), "*.dll", plugin_auto_load_cb);
#else
#if defined(__hpux)
	for_files (RAGELIBDIR"/plugins", "*.sl", plugin_auto_load_cb);
	for_files (get_xdir_fs (), "*.sl", plugin_auto_load_cb);
#else
	for_files (RAGELIBDIR"/plugins", "*.so", plugin_auto_load_cb);
	for_files (get_xdir_fs (), "*.so", plugin_auto_load_cb);
#endif
#endif
}

#endif

static GSList *
plugin_hook_find (GSList *list, int type, char *name)
{
	rage_hook *hook;

	while (list)
	{
		hook = list->data;
		if (hook->type == type)
		{
			if (strcasecmp (hook->name, name) == 0)
				return list;

			if (type == HOOK_SERVER)
			{
				if (strcasecmp (hook->name, "RAW LINE") == 0)
					return list;
			}
		}
		list = list->next;
	}

	return NULL;
}

/* check for plugin hooks and run them */

static int
plugin_hook_run (rage_session *sess, char *name, int parc, char *parv[], int type)
{
	GSList *list, *next;
	rage_hook *hook;
	int ret, eat = 0;

	list = hook_list[type];
	while (1)
	{
		list = plugin_hook_find (list, type, name);
		if (!list)
			goto xit;

		hook = list->data;
		next = list->next;
		hook->pl->context = sess;

		/* run the plugin's callback function */
		switch (type)
		{
			case HOOK_COMMAND:
				ret = ((rage_cmd_cb *)hook->callback) (parc, parv, hook->userdata);
				break;
			case HOOK_SERVER:
				ret = ((rage_serv_cb *)hook->callback) (parc, parv, hook->userdata);
				break;
			case HOOK_PRINT:
				ret = ((rage_print_cb *)hook->callback) (parc, parv, hook->userdata);
				break;
			default:
				ret = 0;
				break;
		}

		if ((ret & RAGE_EAT_RAGE) && (ret & RAGE_EAT_PLUGIN))
		{
			eat = 1;
			goto xit;
		}
		if (ret & RAGE_EAT_PLUGIN)
			goto xit;	/* stop running plugins */
		if (ret & RAGE_EAT_RAGE)
			eat = 1;	/* eventually we'll return 1, but continue running plugins */

		list = next;
	}

xit:
	/* really remove deleted hooks now */

	return eat;
}

/* execute a plugged in command. Called from outbound.c */

int
plugin_emit_command (rage_session *sess, char *name, char *buf)
{
	char *parv[3] = { name, buf, "" };
	
	return plugin_hook_run (sess, name, 2, parv, HOOK_COMMAND);
}

/* got a server PRIVMSG, NOTICE, numeric etc... */

int
plugin_emit_server (rage_session *sess, char *name, int parc, char *parv[])
{
	return plugin_hook_run (sess, name, parc, parv, HOOK_SERVER);
}

/* see if any plugins are interested in this print event */

int
plugin_emit_print (rage_session *sess, int parc, char *parv[])
{
	return plugin_hook_run (sess, parv[0], parc, parv, HOOK_PRINT);
}

int
plugin_emit_dummy_print (rage_session *sess, char *name)
{
	char *word[32];
	int i;

	word[0] = name;
	for (i = 1; i < 32; i++)
		word[i] = "\000";

	return plugin_hook_run (sess, name, 1, word, HOOK_PRINT);
}

static int
plugin_timeout_cb (rage_hook *hook)
{
	int ret;

	/* timer_cb's context starts as front-most-tab */
	hook->pl->context = current_sess;

	/* call the plugin's timeout function */
	ret = ((rage_timer_cb *)hook->callback) (hook->userdata);

	/* the callback might have already unhooked it! */
	if (!g_slist_find (hook_list[hook->type], hook))
		return 0;

	if (ret == 0)
	{
		hook->tag = 0;	/* avoid g_source_remove, returning 0 is enough! */
		rage_unhook (hook->pl, hook);
	}

	return ret;
}

/* insert a hook into hook_list according to its priority */

static void
plugin_insert_hook (rage_hook *new_hook)
{
	GSList *list;
	rage_hook *hook;

	list = hook_list[new_hook->type];
	while (list)
	{
		hook = list->data;
		if (hook->pri <= new_hook->pri)
		{
			hook_list[new_hook->type] = 
				g_slist_insert_before (hook_list[new_hook->type], list, new_hook);
			return;
		}
		list = list->next;
	}

	hook_list[new_hook->type] = g_slist_append (hook_list[new_hook->type], new_hook);
}

static gboolean
plugin_fd_cb (GIOChannel *source, GIOCondition condition, rage_hook *hook)
{
	int flags = 0;
	typedef int (rage_fd_cb2) (int fd, int flags, void *user_data, GIOChannel *);

	if (condition & G_IO_IN)
		flags |= RAGE_FD_READ;
	if (condition & G_IO_OUT)
		flags |= RAGE_FD_WRITE;
	if (condition & G_IO_PRI)
		flags |= RAGE_FD_EXCEPTION;

	return ((rage_fd_cb2 *)hook->callback) (hook->pri, flags, hook->userdata, source);
}

/* allocate and add a hook to our list. Used for all 4 types */

static rage_hook *
plugin_add_hook (rage_plugin *pl, int type, int pri, const char *name,
					  const  char *help_text, void *callb, int timeout, void *userdata)
{
	rage_hook *hook;

	hook = malloc (sizeof (rage_hook));
	memset (hook, 0, sizeof (rage_hook));

	hook->type = type;
	hook->pri = pri;
	if (name)
		hook->name = strdup (name);
	if (help_text)
		hook->help_text = strdup (help_text);
	hook->callback = callb;
	hook->pl = pl;
	hook->userdata = userdata;

	/* insert it into the linked list */
	plugin_insert_hook (hook);

	if (type == HOOK_TIMER)
		hook->tag = g_timeout_add (timeout, 
				(GSourceFunc)plugin_timeout_cb, hook);

	return hook;
}

GList *
plugin_command_list(GList *tmp_list)
{
	rage_hook *hook;
	GSList *list = hook_list[HOOK_COMMAND];

	while (list)
	{
		hook = list->data;
		tmp_list = g_list_prepend(tmp_list, hook->name);
		list = list->next;
	}
	return tmp_list;
}

int
plugin_show_help (rage_session *sess, char *cmd)
{
	GSList *list;
	rage_hook *hook;

	/* show all help commands */
	if (cmd == NULL)
	{
		list = hook_list[HOOK_COMMAND];
		while (list)
		{
			hook = list->data;
			PrintText (sess, hook->name);
			list = list->next;
		}
		return 1;
	}

	list = plugin_hook_find (hook_list[HOOK_COMMAND], HOOK_COMMAND, cmd);
	if (list)
	{
		hook = list->data;
		if (hook->help_text)
		{
			PrintText (sess, hook->help_text);
			return 1;
		}
	}

	return 0;
}

/* ========================================================= */
/* ===== these are the functions plugins actually call ===== */
/* ========================================================= */

void *
rage_unhook (rage_plugin *ph, rage_hook *hook)
{
	char *data;

	if (!g_slist_find (hook_list[hook->type], hook))
		return NULL;

	switch(hook->type)
	{
		case HOOK_DELETED:
			return NULL;
		case HOOK_TIMER:
		case HOOK_FD:
			if (hook->tag != 0)
				g_source_remove (hook->tag);
		case HOOK_SERVER:
		case HOOK_PRINT:
		case HOOK_COMMAND:
			hook_list[hook->type] = g_slist_remove(hook_list[hook->type], hook);
			break;
	}

	if (hook->name)
		free (hook->name);	/* NULL for timers & fds */
	if (hook->help_text)
		free (hook->help_text);	/* NULL for non-commands */

	data = hook->userdata;
	free(hook);

	return data;
}

rage_hook *
rage_hook_command (rage_plugin *ph, const char *name, int pri,
		rage_cmd_cb *callb, const char *help_text, void *userdata)
{
	return plugin_add_hook (ph, HOOK_COMMAND, pri, name, help_text, callb, 0,
									userdata);
}

rage_hook *
rage_hook_server (rage_plugin *ph, const char *name, int pri,
				 rage_serv_cb *callb, void *userdata)
{
	return plugin_add_hook (ph, HOOK_SERVER, pri, name, 0, callb, 0, userdata);
}

rage_hook *
rage_hook_print (rage_plugin *ph, const char *name, int pri,
						rage_print_cb *callb, void *userdata)
{
	return plugin_add_hook (ph, HOOK_PRINT, pri, name, 0, callb, 0, userdata);
}

rage_hook *
rage_hook_timer (rage_plugin *ph, int timeout, rage_timer_cb *callb,
					   void *userdata)
{
	return plugin_add_hook (ph, HOOK_TIMER, 0, 0, 0, callb, timeout, userdata);
}

rage_hook *
rage_hook_fd (rage_plugin *ph, int fd, int flags,
					rage_fd_cb *callb, void *userdata)
{
	rage_hook *hook;

	hook = plugin_add_hook (ph, HOOK_FD, 0, 0, 0, callb, 0, userdata);
	hook->pri = fd;
	/* plugin hook_fd flags correspond exactly to FIA_* flags (fe.h) */
	hook->tag = net_input_add (fd, flags, plugin_fd_cb, hook);

	return hook;
}

void
rage_print (rage_plugin *ph, const char *text)
{
	if (!is_session (ph->context))
	{
		DEBUG(PrintTextf(0, "%s\trage_print called without a valid context.\n", ph->name));
		return;
	}

	PrintText (ph->context, (char *)text);
}

void
rage_printf (rage_plugin *ph, const char *format, ...)
{
	va_list args;
	char *buf;

	va_start (args, format);
	buf = g_strdup_vprintf (format, args);
	va_end (args);

	rage_print (ph, buf);
	g_free (buf);
}

void
rage_command (rage_plugin *ph, const char *command)
{
	char *buf;
	if (!is_session (ph->context))
	{
		DEBUG(PrintTextf(0, "%s\trage_command called without a valid context.\n", ph->name));
		return;
	}

	buf=strdup(command);
	handle_command (ph->context, buf, FALSE);
	free(buf);
}

void
rage_commandf (rage_plugin *ph, const char *format, ...)
{
	va_list args;
	char *buf;

	va_start (args, format);
	buf = g_strdup_vprintf (format, args);
	va_end (args);

	rage_command (ph, buf);
	g_free (buf);
}

int
rage_nickcmp (rage_plugin *ph, const char *s1, const char *s2)
{
	return ((rage_session *)ph->context)->server->p_cmp (s1, s2);
}

rage_context *
rage_get_context (rage_plugin *ph)
{
	return ph->context;
}

int
rage_set_context (rage_plugin *ph, rage_context *context)
{
	if (is_session (context))
	{
		ph->context = context;
		return 1;
	}
	return 0;
}

rage_context *
rage_find_context (rage_plugin *ph, const char *servname, const char *channel)
{
	GSList *slist, *clist;
	server *serv;
	rage_session *sess;
	char *netname;

	if (servname == NULL && channel == NULL)
		return current_sess;

	slist = serv_list;
	while (slist)
	{
		serv = slist->data;
		netname = get_network (serv->front_session, TRUE);

		if (servname == NULL ||
			 rfc_casecmp (servname, serv->servername) == 0 ||
			 strcasecmp (servname, serv->hostname) == 0 ||
			 strcasecmp (servname, netname) == 0)
		{
			if (channel == NULL)
				return serv->front_session;

			clist = sess_list;
			while (clist)
			{
				sess = clist->data;
				if (sess->server == serv)
				{
					if (rfc_casecmp (channel, sess->channel) == 0)
						return sess;
				}
				clist = clist->next;
			}
		}
		slist = slist->next;
	}

	return NULL;
}

const char *
rage_get_info (rage_plugin *ph, const char *id)
{
	rage_session *sess;

	sess = ph->context;
	if (!is_session (sess))
	{
		DEBUG(PrintTextf(0, "%s\trage_get_info called without a valid context.\n", ph->name));
		return NULL;
	}

	switch (str_hash (id))
	{
		case 0x2de2ee: /* away */
			if (sess->server->is_away)
				return sess->server->last_away_reason;
			return NULL;

	  	case 0x2c0b7d03: /* channel */
			return sess->channel;

		case 0x30f5a8: /* host */
			return sess->server->hostname;

		case 0x1c0e99c1: /* inputbox */
			return fe_get_inputbox_contents (sess);

		case 0x325acab5:	/* libdirfs */
			return RAGELIBDIR;

		case 0x6de15a2e:	/* network */
			return get_network (sess, FALSE);

		case 0x339763: /* nick */
			return sess->server->nick;

		case 0xca022f43: /* server */
			if (!sess->server->connected)
				return NULL;
			return sess->server->servername;

		case 0x696cd2f: /* topic */
			return sess->topic;

		case 0x14f51cd8: /* version */
			return VERSION;

		case 0x6d3431b5: /* win_status */
			switch (fe_gui_info (sess, 0))	/* check window status */
			{
				case 0: return "normal";
				case 1: return "active";
				case 2: return "hidden";
			}
			return NULL;

		case 0xdd9b1abd:	/* ragedir */
			return get_xdir_utf8 ();

		case 0xe33f6c4a:	/* ragedirfs */
			return get_xdir_fs ();
	}

	return NULL;
}

int
rage_get_prefs (rage_plugin *ph, const char *name, const char **string, int *integer)
{
	int i = 0;

	do
	{
		if (!strcasecmp (name, vars[i].name))
		{
			switch (vars[i].type)
			{
				case TYPE_STR:
					*string = ((char *) &prefs + vars[i].offset);
					return 1;

				case TYPE_INT:
					*integer = *((int *) &prefs + vars[i].offset);
					return 2;

				default:
				/*case TYPE_BOOL:*/
					if (*((int *) &prefs + vars[i].offset))
						*integer = 1;
					else
						*integer = 0;
					return 3;
			}
		}
		i++;
	}
	while (vars[i].name);

	return 0;
}

rage_list *
rage_list_get (rage_plugin *ph, const char *name)
{
	rage_list *list;

	list = malloc (sizeof (rage_list));
	list->pos = NULL;

	switch (str_hash (name))
	{
		case 0x556423d0: /* channels */
			list->type = LIST_CHANNELS;
			list->next = sess_list;
			break;

		case 0x183c4:	/* dcc */
			list->type = LIST_DCC;
			list->next = dcc_list;
			break;

		case 0xb90bfdd2:	/* ignore */
			list->type = LIST_IGNORE;
			list->next = ignore_list;
			break;

		case 0xc2079749:	/* notify */
			list->type = LIST_NOTIFY;
			list->next = notify_list;
			list->head = (void *)ph->context;	/* reuse this pointer */
			break;

		case 0x6a68e08: /* users */
			if (is_session (ph->context))
			{
				list->type = LIST_USERS;
				list->head = list->next = userlist_flat_list (ph->context);
				break;
			}	/* fall through */

		default:
			free (list);
			return NULL;
	}

	return list;
}

void
rage_list_free (rage_plugin *ph, rage_list *xlist)
{
	if (xlist->type == LIST_USERS)
		g_slist_free (xlist->head);
	free (xlist);
}

int
rage_list_next (rage_plugin *ph, rage_list *xlist)
{
	if (xlist->next == NULL)
		return 0;

	xlist->pos = xlist->next;
	xlist->next = xlist->pos->next;

	/* NOTIFY LIST: Find the entry which matches the context
		of the plugin when list_get was originally called. */
	if (xlist->type == LIST_NOTIFY)
	{
		xlist->notifyps = notify_find_server_entry (xlist->pos->data,
				((rage_session *)xlist->head)->server);
		if (!xlist->notifyps)
			return 0;
	}

	return 1;
}

const char **
rage_list_fields (rage_plugin *ph, const char *name)
{
	static const char *dcc_fields[] =
	{
		"iaddress32",	"icps",		"sdestfile",	"sfile",	"snick",	"iport",
		"ipos",		"iresume",	"isize",	"istatus", 	"itype",	NULL
	};
	static const char *channels_fields[] =
	{
		"schannel",	"schantypes", 	"pcontext",	"iflags",	"iid",	"imaxmodes",
		"snetwork", 	"snickmodes", 	"snickprefixes","sserver",	"itype","iusers",
		NULL
	};
	static const char *ignore_fields[] =
	{
		"iflags", "smask", NULL
	};
	static const char *notify_fields[] =
	{
		"iflags", "snick", "toff", "ton", "tseen", NULL
	};
	static const char *users_fields[] =
	{
		"iaway", "shost", "snick", "sprefix", NULL
	};
	static const char *list_of_lists[] =
	{
		"channels",	"dcc", "ignore", "notify", "users", NULL
	};

	switch (str_hash (name))
	{
		case 0x556423d0:	/* channels */
			return channels_fields;
		case 0x183c4:		/* dcc */
			return dcc_fields;
		case 0xb90bfdd2:	/* ignore */
			return ignore_fields;
		case 0xc2079749:	/* notify */
			return notify_fields;
		case 0x6a68e08:	/* users */
			return users_fields;
		case 0x6236395:	/* lists */
			return list_of_lists;
	}

	return NULL;
}

time_t
rage_list_time (rage_plugin *ph, rage_list *xlist, const char *name)
{
	guint32 hash = str_hash (name);
/*	gpointer data = xlist->pos->data;*/

	switch (xlist->type)
	{
		case LIST_NOTIFY:
			if (!xlist->notifyps)
				return (time_t) -1;
		switch (hash)
		{
			case 0x1ad6f:	/* off */
				return xlist->notifyps->lastoff;
			case 0xddf:	/* on */
				return xlist->notifyps->laston;
			case 0x35ce7b:	/* seen */
				return xlist->notifyps->lastseen;
		}
	}

	return (time_t) -1;
}

const char *
rage_list_str (rage_plugin *ph, rage_list *xlist, const char *name)
{
	guint32 hash = str_hash (name);
	gpointer data = xlist->pos->data;

	switch (xlist->type)
	{
		case LIST_CHANNELS:
			switch (hash)
			{
				case 0x2c0b7d03: /* channel */
					return ((rage_session *)data)->channel;
				case 0x577e0867: /* chantypes */
					return get_isupport(ph->context->server, "CHANTYPES");
				case 0x38b735af: /* context */
					return data;	/* this is a rage_session * */
				case 0x6de15a2e: /* network */
					return get_network ((rage_session *)data, FALSE);
				case 0x8455e723: /* nickprefixes */
					return get_isupport(ph->context->server, "PREFIX");
				case 0x829689ad: /* nickmodes */
				{
					char *str = strchr(get_isupport(ph->context->server, 
								"PREFIX"), ')');
					if(str)
						*str++;
					return str ? str : "";
				}
				case 0xca022f43: /* server */
					return ((rage_session *)data)->server->servername;
			}
			break;

		case LIST_DCC:
			switch (hash)
			{
				case 0x3d9ad31e:	/* destfile */
					return ((struct DCC *)data)->destfile;
				case 0x2ff57c:	/* file */
					return ((struct DCC *)data)->file;
				case 0x339763: /* nick */
					return ((struct DCC *)data)->nick;
			}
			break;

		case LIST_IGNORE:
			switch (hash)
			{
				case 0x3306ec:	/* mask */
					return ((struct ignore *)data)->mask;
			}
			break;

		case LIST_NOTIFY:
			switch (hash)
			{
				case 0x339763: /* nick */
					return ((struct notify *)data)->name;
			}
			break;

		case LIST_USERS:
			switch (hash)
			{
				case 0x339763: /* nick */
					return ((struct User *)data)->nick;
				case 0x30f5a8: /* host */
					return ((struct User *)data)->hostname;
				case 0xc594b292: /* prefix */
					return ((struct User *)data)->prefix;
			}
			break;
	}

	return NULL;
}

int
rage_list_int (rage_plugin *ph, rage_list *xlist, const char *name)
{
	guint32 hash = str_hash (name);
	gpointer data = xlist->pos->data;
	int tmp = 0;

	switch (xlist->type)
	{
		case LIST_DCC:
			switch (hash)
			{
				case 0x34207553: /* address32 */
					return ((struct DCC *)data)->addr;
				case 0x181a6: /* cps */
					return ((struct DCC *)data)->cps;
				case 0x349881: /* port */
					return ((struct DCC *)data)->port;
				case 0x1b254: /* pos */
					return ((struct DCC *)data)->pos;
				case 0xc84dc82d: /* resume */
					return ((struct DCC *)data)->resumable;
				case 0x35e001: /* size */
					return ((struct DCC *)data)->size;
				case 0xcacdcff2: /* status */
					return ((struct DCC *)data)->dccstat;
				case 0x368f3a: /* type */
					return ((struct DCC *)data)->type;
			}
			break;

		case LIST_IGNORE:
			switch (hash)
			{
				case 0x5cfee87:	/* flags */
					return ((struct ignore *)data)->type;
			}	
			break;

		case LIST_CHANNELS:
			switch (hash)
			{
				case 0xd1b:	/* id */
					return ((rage_session *)data)->server->id;
				case 0x5cfee87:	/* flags */
					tmp = isupport(ph->context->server, "WHOX");  /* bit 4 */
					tmp <<= 1;
					tmp |= ((rage_session *)data)->server->end_of_motd;/* 3 */
					tmp <<= 1;
					tmp |= ((rage_session *)data)->server->is_away;    /* 2 */
					tmp <<= 1;
					tmp |= ((rage_session *)data)->server->connecting; /* 1 */ 
					tmp <<= 1;
					tmp |= ((rage_session *)data)->server->connected;  /* 0 */
					return tmp;
				case 0x1916144c: /* maxmodes */
					return atoi(get_isupport(ph->context->server, "MODES"));
				case 0x368f3a:	/* type */
					return ((rage_session *)data)->type;
				case 0x6a68e08: /* users */
					return ((rage_session *)data)->total;
			}
			break;

		case LIST_NOTIFY:
			if (!xlist->notifyps)
				return -1;
			switch (hash)
			{
				case 0x5cfee87: /* flags */
					return xlist->notifyps->ison;
			}

		case LIST_USERS:
			switch (hash)
			{
				case 0x2de2ee:	/* away */
					return ((struct User *)data)->away;
			}
			break;

	}

	return -1;
}

void *
rage_plugingui_add (rage_plugin *ph, const char *filename,
		const char *name, const char *desc,
		const char *version, char *reserved)
{
#ifdef USE_PLUGIN
	ph = plugin_list_add (NULL, strdup (filename), name, desc, version, NULL,
								 NULL, TRUE);
	fe_pluginlist_update ();
#endif

	return ph;
}

void
rage_plugingui_remove (rage_plugin *ph, void *handle)
{
#ifdef USE_PLUGIN
	plugin_free (handle, FALSE, FALSE);
#endif
}

int
rage_emit_print (rage_plugin *ph, const char *event_name, ...)
{
	va_list args;
	char *argv[4] = {NULL, NULL, NULL, NULL};
	int i = 0;

	memset (&argv, 0, sizeof (argv));
	va_start (args, event_name);
	while (1)
	{
		argv[i] = va_arg (args, char *);
		if (!argv[i])
			break;
		i++;
		if (i >= 4)
			break;
	}

	i = text_emit_by_name ((char *)event_name, ph->context, argv[0], argv[1],
								  argv[2], argv[3]);
	va_end (args);

	return i;
}

char *
rage_gettext (rage_plugin *ph, const char *msgid)
{
	/* so that plugins can use rage's internal gettext strings. */
	/* e.g. The EXEC plugin uses this on Windows. */
	return _(msgid);
}

void
rage_send_modes (rage_plugin *ph, const char **targets, int ntargets, 
		char sign, char mode)
{
	send_channel_modes (ph->context, (char **)targets, 0, ntargets, sign, mode);
}

