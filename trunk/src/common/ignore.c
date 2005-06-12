/* X-Chat
 * Copyright (C) 1998 Peter Zelezny.
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

#include "rage.h"

int ignored_ctcp = 0;			  /* keep a count of all we ignore */
int ignored_priv = 0;
int ignored_chan = 0;
int ignored_noti = 0;
int ignored_invi = 0;
static int ignored_total = 0;

/* ignore_exists ():
 * returns: struct ig, if this mask is in the ignore list already
 *          NULL, otherwise
 */
struct ignore *
ignore_exists (char *mask)
{
	struct ignore *ig = 0;
	GSList *list;

	list = ignore_list;
	while (list)
	{
		ig = (struct ignore *) list->data;
		if (!rfc_casecmp (ig->mask, mask))
			return ig;
		list = list->next;
	}
	return NULL;

}

/* ignore_add(...)

 * returns:
 *            0 fail
 *            1 success
 *            2 success (old ignore has been changed)
 */

int
ignore_add (char *mask, int type)
{
	struct ignore *ig = 0;
	int change_only = FALSE;

	/* first check if it's already ignored */
	ig = ignore_exists (mask);
	if (ig)
		change_only = TRUE;

	if (!change_only)
		ig = malloc (sizeof (struct ignore));

	if (!ig)
		return 0;

	ig->mask = strdup (mask);
	ig->type = type;

	if (!change_only)
		ignore_list = g_slist_prepend (ignore_list, ig);
	fe_ignore_update (1);

	if (change_only)
		return 2;

	return 1;
}

void
ignore_showlist (rage_session *sess)
{
	struct ignore *ig;
	GSList *list = ignore_list;
	char tbuf[256];
	int i = 0;

	EMIT_SIGNAL (XP_TE_IGNOREHEADER, sess, 0, 0, 0, 0, 0);

	while (list)
	{
		ig = list->data;
		i++;

		snprintf (tbuf, sizeof (tbuf), " %-25s ", ig->mask);
		if (ig->type & IG_PRIV)
			strcat (tbuf, _("YES  "));
		else
			strcat (tbuf, _("NO   "));
		if (ig->type & IG_NOTI)
			strcat (tbuf, _("YES  "));
		else
			strcat (tbuf, _("NO   "));
		if (ig->type & IG_CHAN)
			strcat (tbuf, _("YES  "));
		else
			strcat (tbuf, _("NO   "));
		if (ig->type & IG_CTCP)
			strcat (tbuf, _("YES  "));
		else
			strcat (tbuf, _("NO   "));
		if (ig->type & IG_DCC)
			strcat (tbuf, _("YES  "));
		else
			strcat (tbuf, _("NO   "));
		if (ig->type & IG_INVI)
			strcat (tbuf, _("YES  "));
		else
			strcat (tbuf, _("NO   "));
		if (ig->type & IG_UNIG)
			strcat (tbuf, _("YES  "));
		else
			strcat (tbuf, _("NO   "));
		strcat (tbuf, "\n");
		PrintText (sess, tbuf);
		/*EMIT_SIGNAL (XP_TE_IGNORELIST, sess, ig->mask, 0, 0, 0, 0); */
		/* use this later, when TE's support 7 args */
		list = list->next;
	}

	if (!i)
		EMIT_SIGNAL (XP_TE_IGNOREEMPTY, sess, 0, 0, 0, 0, 0);

	EMIT_SIGNAL (XP_TE_IGNOREFOOTER, sess, 0, 0, 0, 0, 0);
}

/* ignore_del()

 * one of the args must be NULL, use mask OR *ig, not both
 *
 */

int
ignore_del (char *mask, struct ignore *ig)
{
	if (!ig)
	{
		GSList *list = ignore_list;

		while (list)
		{
			ig = (struct ignore *) list->data;
			if (!rfc_casecmp (ig->mask, mask))
				break;
			list = list->next;
			ig = 0;
		}
	}
	if (ig)
	{
		ignore_list = g_slist_remove (ignore_list, ig);
		free (ig->mask);
		free (ig);
		fe_ignore_update (1);
		return TRUE;
	}
	return FALSE;
}

/* check if a msg should be ignored by browsing our ignore list */

int
ignore_check (char *host, int type)
{
	struct ignore *ig;
	GSList *list = ignore_list;

	/* check if there's an UNIGNORE first, they take precendance. */
	while (list)
	{
		ig = (struct ignore *) list->data;
		if (ig->type & type)
		{
			if (match (ig->mask, host))
			{
				if (ig->type & IG_UNIG)	
					return FALSE;
				else
				{
					ignored_total++;
					if (type & IG_PRIV)
						ignored_priv++;
					if (type & IG_NOTI)
						ignored_noti++;
					if (type & IG_CHAN)
						ignored_chan++;
					if (type & IG_CTCP)
						ignored_ctcp++;
					if (type & IG_INVI)
						ignored_invi++;
					fe_ignore_update (2);
					return TRUE;
				}
			}
		}
		list = list->next;
	}

	return FALSE;
}

static char *
ignore_read_next_entry (char *my_cfg, struct ignore *ignore)
{
	char tbuf[1024];

	/* Casting to char * done below just to satisfy compiler */

	if (my_cfg)
	{
		my_cfg = cfg_get_str (my_cfg, "mask", tbuf, sizeof (tbuf));
		if (!my_cfg)
			return NULL;
		ignore->mask = strdup (tbuf);
	}
	if (my_cfg)
	{
		my_cfg = cfg_get_str (my_cfg, "type", tbuf, sizeof (tbuf));
		ignore->type = atoi (tbuf);
	}
	return my_cfg;
}

void
ignore_load ()
{
	struct ignore *ignore;
	struct stat st;
	char *cfg, *my_cfg;
	char file[256];
	int fh, i;

	snprintf (file, sizeof (file), "%s/ignore.conf", get_xdir_fs ());
	fh = open (file, O_RDONLY | OFLAGS);
	if (fh != -1)
	{
		fstat (fh, &st);
		if (st.st_size)
		{
			cfg = malloc ((int)st.st_size + 1);
			cfg[0] = '\0';
			i = read (fh, cfg, (int)st.st_size);
			if (i >= 0)
				cfg[i] = '\0';
			my_cfg = cfg;
			while (my_cfg)
			{
				ignore = malloc (sizeof (struct ignore));
				memset (ignore, 0, sizeof (struct ignore));
				if ((my_cfg = ignore_read_next_entry (my_cfg, ignore)))
					ignore_list = g_slist_prepend (ignore_list, ignore);
				else
					free (ignore);
			}
			free (cfg);
		}
		close (fh);
	}
}

void
ignore_save ()
{
	char buf[1024];
	int fh;
	GSList *temp = ignore_list;
	struct ignore *ig;

	snprintf (buf, sizeof (buf), "%s/ignore.conf", get_xdir_fs ());
	fh = open (buf, O_TRUNC | O_WRONLY | O_CREAT | OFLAGS, 0600);
	if (fh != -1)
	{
		while (temp)
		{
			ig = (struct ignore *) temp->data;
			if (!(ig->type & IG_NOSAVE))
			{
				snprintf (buf, sizeof (buf), "mask = %s\ntype = %d\n\n",
							 ig->mask, ig->type);
				write (fh, buf, (int)strlen (buf));
			}
			temp = temp->next;
		}
		close (fh);
	}

}

int
flood_check (char *nick, char *host, server *serv, rage_session *sess, int what)	/*0=ctcp  1=priv */
{
	char buf[512], real_ip[132];
	int flood = 0, value;

	if (what == 0 )
	{
		value = 10;
		if (prefs.ctcp_number_limit > 0 && 
				queue_count(serv, nick, 2) > prefs.ctcp_number_limit) 
		{
			char *tmp = strchr(host, '@');
			snprintf (real_ip, sizeof (real_ip), "*!*%s", tmp);
			/* Clean the queue and add the ignore first to avoid
			 * stating the ignore several times. */
			queue_remove_replies(serv, nick);
			ignore_add (real_ip, IG_CTCP);

			snprintf (buf, sizeof (buf),
					_("You are being CTCP flooded from %s, ignoring %s\n"), 
					nick, real_ip);
			flood = 1;
		}
	}
	else
	{
		value = 1;
		/* FIXME: (signed int *) -- Is this the best way to fix this? */
		if (prefs.msg_number_limit > 0 && gen_parm_throttle ((signed int *)&(serv->msg_counter), &value, &value, 
					(signed int *)&(prefs.msg_number_limit), &serv->msg_last_time))
		{
			serv->msg_counter = 0;

			snprintf (buf, sizeof (buf),
					_("You are being MSG flooded from %s, setting gui_auto_open_dialog OFF.\n"), host);
			prefs.autodialog = 0;
			flood = 1;
		}
	}
	
	if (flood)
	{
		PrintText (sess, buf);
		return 0;
	}
	return 1;
}

