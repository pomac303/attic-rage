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

#define _GNU_SOURCE	/* for memrchr, not used on Windows */
#include "rage.h"

static int cmd_server (session *sess, char *cmd, char *buf);
static void help (session *sess, char *helpcmd, int quiet);
static void handle_say (session *sess, char *text, int check_spch);

dict_t rage_cmd_list = NULL;

static void
notj_msg (struct session *sess)
{
	PrintText (sess, _("No channel joined. Try /join #<channel>\n"));
}

void
notc_msg (struct session *sess)
{
	PrintText (sess, _("Not connected. Try /server <host> [<port>]\n"));
}

static char *
random_line (char *file_name)
{
	FILE *fh;
	char buf[512];
	int lines, ran;

	if (!file_name[0])
		goto nofile;

	snprintf (buf, sizeof (buf), "%s/%s", get_xdir_fs (), file_name);
	fh = fopen (buf, "r");
	if (!fh)
	{
	 nofile:
		/* reason is not a file, an actual reason! */
		return strdup (file_name);
	}

	/* count number of lines in file */
	lines = 0;
	while (fgets (buf, sizeof (buf), fh))
		lines++;

	if (lines < 1)
		goto nofile;

	/* go down a random number */
	rewind (fh);
	srand ((unsigned int)time(0));
	ran = rand () % lines;
	do
	{
		fgets (buf, sizeof (buf), fh);
		lines--;
	}
	while (lines > ran);
	fclose (fh);
	buf[strlen (buf) - 1] = 0;	  /* remove the trailing '\n' */
	return strdup (buf);
}

void
server_sendpart (server * serv, char *channel, char *reason)
{
	if (!reason)
	{
		reason = random_line (prefs.partreason);
		serv->p_part (serv, channel, reason);
		free (reason);
	} else
	{
		/* reason set by /quit, /close argument */
		serv->p_part (serv, channel, reason);
	}
}

void
server_sendquit (session * sess)
{
	char *rea, *colrea;

	if (!sess->quitreason)
	{
		colrea = strdup (prefs.quitreason);
		check_special_chars (colrea, FALSE);
		rea = random_line (colrea);
		free (colrea);
		sess->server->p_quit (sess->server, rea);
		free (rea);
	} else
	{
		/* reason set by /quit, /close argument */
		sess->server->p_quit (sess->server, sess->quitreason);
	}
}

void
process_data_init (char *buf, char *cmd, int parc,
						 char *parv[], int handle_quotes)
{
	int wordcount = 2;
	int space = FALSE;
	int quote = FALSE;
	int j = 0;
	int len;

	parv[0] = "\000\000";
	//word_eol[0] = "\000\000";
	parv[0] = (char *)buf;
	//word_eol[1] = (char *)cmd;

	while (1)
	{
		switch (*cmd)
		{
		case 0:
		 jump:
			buf[j] = 0;
			for (j = wordcount; j < PDIWORDS; j++)
			{
				parv[j] = "\000\000";
				//word_eol[j] = "\000\000";
			}
			return;
		case '\042':
			if (!handle_quotes)
				goto def;
			if (quote)
			{
				quote = FALSE;
				space = FALSE;
			} else
				quote = TRUE;
			cmd++;
			break;
		case ' ':
			if (!quote)
			{
				if (!space)
				{
					buf[j] = 0;
					j++;
					/* word */
					parv[wordcount] = &buf[j];
					//word_eol[wordcount] = cmd + 1;
					wordcount++;

					if (wordcount == PDIWORDS - 1)
						goto jump;

					space = TRUE;
				}
				cmd++;
				break;
			}
		default:
def:
			space = FALSE;
			len = g_utf8_skip[((unsigned char *)cmd)[0]];
			if (len == 1)
			{
				buf[j] = *cmd;
				j++;
				cmd++;
			} else
			{
				/* skip past a multi-byte utf8 char */
				memcpy (buf + j, cmd, len);
				j += len;
				cmd += len;
			}
		}
	}
}

static int
cmd_addbutton (struct session *sess, char *cmd, char *buf)
{
	char *parv[MAX_TOKENS];
	int parc;
	split_cmd_parv(buf,&parc,parv);
	if (*parv[1] && *parv[2])
	{
		if (sess->type == SESS_DIALOG)
		{
			list_addentry (&dlgbutton_list, parv[2], parv[1]);
			fe_dlgbuttons_update (sess);
		} else
		{
			list_addentry (&button_list, parv[2], parv[1]);
			fe_buttons_update (sess);
		}
		return TRUE;
	}
	return FALSE;
}

static int
cmd_allchannels (session *sess, char *cmd, char *buf)
{
	GSList *list = sess_list;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	if (!*parv[1])
		return FALSE;

	while (list)
	{
		sess = list->data;
		if (sess->type == SESS_CHANNEL && sess->channel[0] && sess->server->connected)
		{
			handle_command (sess, parv[1], FALSE);
		}
		list = list->next;
	}

	return TRUE;
}

static int
cmd_allservers (struct session *sess, char *cmd, char *buf)
{
	GSList *list;
	server *serv;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	if (!*parv[1])
		return FALSE;

	list = serv_list;
	while (list)
	{
		serv = list->data;
		if (serv->connected)
			handle_command (serv->front_session, parv[1], FALSE);
		list = list->next;
	}

	return TRUE;
}

static int
cmd_away (struct session *sess, char *cmd, char *buf)
{
	GSList *list;
	char *reason;
	int back = FALSE;
	unsigned int gone;
	char send[1024];

	split_cmd(&buf);
	skip_white(&buf);
	reason = buf;

	if (!(*reason) && sess->server->is_away)
	{
		/* mark back */
		sess->server->p_set_back (sess->server);
		back = TRUE;
	} else
	{
		if (!(*reason))
		{
			if (sess->server->reconnect_away)
				reason = sess->server->last_away_reason;
			else
				/* must manage memory pointed to by random_line() */
				reason = random_line (prefs.awayreason);
		}
		sess->server->p_set_away (sess->server, reason);
	}

	if (prefs.show_away_message)
	{
		if (back)
		{
			gone = (unsigned int)time (NULL) - (unsigned int)sess->server->away_time;
			sprintf (send, "me is back (gone %.2d:%.2d:%.2d)", gone / 3600,
						(gone / 60) % 60, gone % 60);
		} else
		{
			sprintf (send, "me is away: %s", reason);
		}

		list = sess_list;
		while (list)
		{
			/* am I the right server and not a dialog box */
			if (((struct session *) list->data)->server == sess->server
				 && ((struct session *) list->data)->type == SESS_CHANNEL
				 && ((struct session *) list->data)->channel[0])
			{
				handle_command ((session *) list->data, send, TRUE);
			}
			list = list->next;
		}
	}

	if (sess->server->last_away_reason != reason)
	{
		free (sess->server->last_away_reason);
		if (reason == buf)
			sess->server->last_away_reason = strdup (reason);
		else
			sess->server->last_away_reason = reason;
	}

	return TRUE;
}

static void
ban (session * sess, char *tbuf, char *mask, char *bantypestr, int deop)
{
	int bantype;
	struct User *user;
	char *at, *dot, *lastdot;
	char username[64], fullhost[128], domain[128], *mode, *p2;
	server *serv = sess->server;

	user = find_name (sess, mask);
	if (user && user->hostname)  /* it's a nickname, let's find a proper ban mask */
	{
		if (deop)
		{
			mode = "-o+b";
			p2 = user->nick;
		} else
		{
			mode = "+b";
			p2 = "";
		}

		mask = user->hostname;

		at = g_utf8_strchr (mask, -1, '@');	/* is utf8 checking really needed? */
		if (!at)
			return;					  /* can't happen? */
		*at = 0;

		if (mask[0] == '~' || mask[0] == '+' ||
		    mask[0] == '=' || mask[0] == '^' || mask[0] == '-')
		{
			safe_strcpy (username, mask+1, sizeof (username));
		} else
		{
			safe_strcpy (username, mask, sizeof (username));
		}
		*at = '@';
		safe_strcpy (fullhost, at + 1, sizeof (fullhost));

		dot = strchr (fullhost, '.');
		if (dot)
		{
			safe_strcpy (domain, dot, sizeof (domain));
		} else
		{
			safe_strcpy (domain, fullhost, sizeof (domain));
		}

		if (*bantypestr)
			bantype = atoi (bantypestr);
		else
			bantype = prefs.bantype;

		tbuf[0] = 0;
		if (inet_addr (fullhost) != -1)	/* "fullhost" is really a IP number */
		{
			lastdot = strrchr (fullhost, '.');
			if (!lastdot)
				return;				  /* can't happen? */

			*lastdot = 0;
			strcpy (domain, fullhost);
			*lastdot = '.';

			switch (bantype)
			{
			case 0:
				sprintf (tbuf, "%s %s *!*@%s.*", mode, p2, domain);
				break;

			case 1:
				sprintf (tbuf, "%s %s *!*@%s", mode, p2, fullhost);
				break;

			case 2:
				sprintf (tbuf, "%s %s *!*%s@%s.*", mode, p2, username, domain);
				break;

			case 3:
				sprintf (tbuf, "%s %s *!*%s@%s", mode, p2, username, fullhost);
				break;
			}
		} else
		{
			switch (bantype)
			{
			case 0:
				sprintf (tbuf, "%s %s *!*@*%s", mode, p2, domain);
				break;

			case 1:
				sprintf (tbuf, "%s %s *!*@%s", mode, p2, fullhost);
				break;

			case 2:
				sprintf (tbuf, "%s %s *!*%s@*%s", mode, p2, username, domain);
				break;

			case 3:
				sprintf (tbuf, "%s %s *!*%s@%s", mode, p2, username, fullhost);
				break;
			}
		}

	} else
	{
		sprintf (tbuf, "+b %s", mask);
	}
	serv->p_mode (serv, sess->channel, tbuf);
}

static int
cmd_ban (struct session *sess, char *cmd, char *buf)
{
	char *mask;
	int parc;
	char *parv[MAX_TOKENS];
	char send[1024];

	split_cmd_parv(buf,&parc,parv);

	mask=parv[1];

	if (*mask)
	{
		ban (sess, send, mask, parv[2], 0);
	} else
	{
		sess->server->p_mode (sess->server, sess->channel, "+b");	/* banlist */
	}

	return TRUE;
}

static int
cmd_unban (struct session *sess, char *cmd, char *buf)
{
	/* Allow more than one mask in /unban -- tvk */
	int i = 1;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	while (1)
	{
		if (!*parv[i])
		{
			if (i == 1)
				return FALSE;
			send_channel_modes (sess, parv, 1, i, '-', 'b');
			return TRUE;
		}
		i++;
	}
}

static int
cmd_charset (struct session *sess, char *cmd, char *buf)
{
	server *serv = sess->server;
	const char *locale = NULL;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	if (!parv[1][0])
	{
		g_get_charset (&locale);
		PrintTextf (sess, "Current charset: %s\n",
						serv->encoding ? serv->encoding : locale);
		return TRUE;
	}

	if (servlist_check_encoding (parv[1]))
	{
		if (serv->encoding)
			free (serv->encoding);
		serv->encoding = strdup (parv[1]);
		PrintTextf (sess, "Charset changed to: %s\n", parv[1]);
	} else
	{
		PrintTextf (sess, "\0034Unknown charset:\017 %s\n", parv[1]);
	}

	return TRUE;
}

static int
cmd_clear (struct session *sess, char *cmd, char *buf)
{
	GSList *list = sess_list;
	char *reason;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	reason = parv[1];

	if (strncasecmp (reason, "all", 3) == 0)
	{
		while (list)
		{
			sess = list->data;
			if (!sess->nick_said)
				fe_text_clear (list->data);
			list = list->next;
		}
	} else
	{
		fe_text_clear (sess);
	}

	return TRUE;
}

static int
cmd_close (struct session *sess, char *cmd, char *buf)
{
	GSList *list;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	if (strcmp (parv[1], "-m") == 0)
	{
		list = sess_list;
		while (list)
		{
			sess = list->data;
			list = list->next;
			if (sess->type == SESS_DIALOG)
				fe_close_window (sess);
		}
	} else
	{
		if (*parv[1])
			sess->quitreason = parv[1];
		fe_close_window (sess);
	}

	return TRUE;
}

static int
cmd_ctcp (struct session *sess, char *cmd, char *buf)
{
	int mbl;
	char *to;
	char *msg;
	split_cmd(&buf);
	to=split_cmd(&buf);
	skip_white(&buf);
	msg=buf;
	if (*to)
	{
		if (*msg)
		{
			unsigned char *cmd = (unsigned char *)msg;
			int parc;
			char *parv[MAX_TOKENS];

			split_cmd_parv(buf,&parc,parv);

			/* make the first word upper case (as per RFC) */
			while (1)
			{
				if (*cmd == ' ' || *cmd == 0)
					break;
				mbl = g_utf8_skip[*cmd];
				if (mbl == 1)
					*cmd = toupper (*cmd);
				cmd += mbl;
			}

			sess->server->p_ctcp (sess->server, to, msg);

			EMIT_SIGNAL (XP_TE_CTCPSEND, sess, to, msg, NULL, NULL, 0);

			return TRUE;
		}
	}
	return FALSE;
}

static int
cmd_country (struct session *sess, char *cmd, char *buf)
{
	char *code;
	int parc;
	char *parv[MAX_TOKENS];
	char tbuf[500];

	split_cmd_parv(buf,&parc,parv);

	code = parv[1];

	if (*code)
	{
		sprintf (tbuf, "%s = %s\n", code, country (code));
		PrintText (sess, tbuf);
		return TRUE;
	}
	return FALSE;
}

static int
cmd_cycle (struct session *sess, char *cmd, char *buf)
{
	char *key = sess->channelkey;
	char *chan = sess->channel;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	if (*chan && sess->type == SESS_CHANNEL)
	{
		sess->server->p_cycle (sess->server, chan, key);
		return TRUE;
	}
	return FALSE;
}

static int
cmd_dcc (struct session *sess, char *cmd, char *buf)
{
	int goodtype;
	struct DCC *dcc = 0;
	char *type;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	type = parv[1];

	if (*type)
	{
		if (!strcasecmp (type, "HELP"))
			return FALSE;
		if (!strcasecmp (type, "CLOSE"))
		{
			if (*parv[2] && *parv[2])
			{
				goodtype = 0;
				if (!strcasecmp (parv[2], "SEND"))
				{
					dcc = find_dcc (parv[3], parv[2], TYPE_SEND);
					dcc_abort (sess, dcc);
					goodtype = TRUE;
				}
				if (!strcasecmp (parv[2], "GET"))
				{
					dcc = find_dcc (parv[3], parv[2], TYPE_RECV);
					dcc_abort (sess, dcc);
					goodtype = TRUE;
				}
				if (!strcasecmp (parv[2], "CHAT"))
				{
					dcc = find_dcc (parv[3], "", TYPE_CHATRECV);
					if (!dcc)
						dcc = find_dcc (parv[3], "", TYPE_CHATSEND);
					dcc_abort (sess, dcc);
					goodtype = TRUE;
				}

				if (!goodtype)
					return FALSE;

				if (!dcc)
					EMIT_SIGNAL (XP_TE_NODCC, sess, NULL, NULL, NULL, NULL, 0);

				return TRUE;

			}
			return FALSE;
		}
		if (!strcasecmp (type, "CHAT"))
		{
			char *nick = parv[2];
			if (*nick)
				dcc_chat (sess, nick);
			return TRUE;
		}
		if (!strcasecmp (type, "LIST"))
		{
			dcc_show_list (sess);
			return TRUE;
		}
		if (!strcasecmp (type, "GET"))
		{
			char *nick = parv[2];
			char *file = parv[3];
			if (!*file)
			{
				if (*nick)
					dcc_get_nick (sess, nick);
			} else
			{
				dcc = find_dcc (nick, file, TYPE_RECV);
				if (dcc)
					dcc_get (dcc);
				else
					EMIT_SIGNAL (XP_TE_NODCC, sess, NULL, NULL, NULL, NULL, 0);
			}
			return TRUE;
		}
		if ((!strcasecmp (type, "SEND")) || (!strcasecmp (type, "PSEND")))
		{
			int i = 2, maxcps;
			char *nick, *file;
			int passive = (!strcasecmp(type, "PSEND")) ? 1 : 0;

			nick = parv[i];
			if (!*nick)
				return FALSE;

			maxcps = prefs.dcc_max_send_cps;
			if (!strncasecmp(nick, "-maxcps=", 8))
			{
				maxcps = atoi(nick + 8);
				i++;
				nick = parv[i];
				if (!*nick)
					return FALSE;
			}

			i++;

			file = parv[i];
			if (!*file)
			{
				fe_dcc_send_filereq (sess, nick, maxcps, passive);
				return TRUE;
			}

			do
			{
				dcc_send (sess, nick, file, maxcps, passive);
				i++;
				file = parv[i];
			}
			while (*file);

			return TRUE;
		}
	} else
		dcc_show_list (sess);
	return TRUE;
}

static int
cmd_debug (struct session *sess, char *cmd, char *buf)
{
	struct session *s;
	struct server *v;
	GSList *list = sess_list;
	int parc;
	char *parv[MAX_TOKENS];
	char tbuf[512];

	split_cmd_parv(buf,&parc,parv);

	PrintText (sess, "Session   T Channel    WaitChan  WillChan  Server\n");
	while (list)
	{
		s = (struct session *) list->data;
		sprintf (tbuf, "%p %1x %-10.10s %-10.10s %-10.10s %p\n",
					s, s->type, s->channel, s->waitchannel,
					s->willjoinchannel, s->server);
		PrintText (sess, tbuf);
		list = list->next;
	}

	list = serv_list;
	PrintText (sess, "Server    Sock  Name\n");
	while (list)
	{
		v = (struct server *) list->data;
		sprintf (tbuf, "%p %-5d %s\n",
					v, v->sok, v->servername);
		PrintText (sess, tbuf);
		list = list->next;
	}

	sprintf (tbuf,
				"\nfront_session: %p\n"
				"current_tab: %p\n\n",
				sess->server->front_session, current_tab);
	PrintText (sess, tbuf);

	return TRUE;
}

static int
cmd_delbutton (struct session *sess, char *cmd, char *buf)
{
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	if (*parv[1])
	{
		if (sess->type == SESS_DIALOG)
		{
			if (list_delentry (&dlgbutton_list, parv[1]))
				fe_dlgbuttons_update (sess);
		} else
		{
			if (list_delentry (&button_list, parv[1]))
				fe_buttons_update (sess);
		}
		return TRUE;
	}
	return FALSE;
}

static int
cmd_dehop (struct session *sess, char *cmd, char *buf)
{
	int i = 1;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	while (1)
	{
		if (!*parv[i])
		{
			if (i == 1)
				return FALSE;
			send_channel_modes (sess, parv, 1, i, '-', 'h');
			return TRUE;
		}
		i++;
	}
}

static int
cmd_deop (struct session *sess, char *cmd, char *buf)
{
	int i = 1;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	while (1)
	{
		if (!*parv[i])
		{
			if (i == 1)
				return FALSE;
			send_channel_modes (sess, parv, 1, i, '-', 'o');
			return TRUE;
		}
		i++;
	}
}

typedef struct
{
	char **nicks;
	int i;
	session *sess;
	char *reason;
	char *tbuf;
} multidata;

static int
mdehop_cb (struct User *user, multidata *data)
{
	if (user->hop && !user->me)
	{
		data->nicks[data->i] = user->nick;
		data->i++;
	}
	return TRUE;
}

static int
cmd_mdehop (struct session *sess, char *cmd, char *buf)
{
	char **nicks = malloc (sizeof (char *) * sess->hops);
	multidata data;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	data.nicks = nicks;
	data.i = 0;
	tree_foreach (sess->usertree, (tree_traverse_func *)mdehop_cb, &data);
	send_channel_modes (sess, nicks, 0, data.i, '-', 'h');
	free (nicks);

	return TRUE;
}

static int
mdeop_cb (struct User *user, multidata *data)
{
	if (user->op && !user->me)
	{
		data->nicks[data->i] = user->nick;
		data->i++;
	}
	return TRUE;
}

static int
cmd_mdeop (struct session *sess, char *cmd, char *buf)
{
	char **nicks = malloc (sizeof (char *) * sess->ops);
	multidata data;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	data.nicks = nicks;
	data.i = 0;
	tree_foreach (sess->usertree, (tree_traverse_func *)mdeop_cb, &data);
	send_channel_modes (sess, nicks, 0, data.i, '-', 'o');
	free (nicks);

	return TRUE;
}

static int
mkick_cb (struct User *user, multidata *data)
{
	if (!user->op && !user->me)
		data->sess->server->p_kick (data->sess->server, data->sess->channel, user->nick, data->reason);
	return TRUE;
}

static int
mkickops_cb (struct User *user, multidata *data)
{
	if (user->op && !user->me)
		data->sess->server->p_kick (data->sess->server, data->sess->channel, user->nick, data->reason);
	return TRUE;
}

/* 
 * Usage: MKICK, Mass kicks everyone except you in the current channel (needs chanop)
 * Pretty lame actually....
 *
 * usage:
 *  /mkick reason
 */
static int
cmd_mkick (struct session *sess, char *cmd, char *buf)
{
	multidata data;

	split_cmd(&buf); /* Skip the command */
	skip_white(&buf);

	data.sess = sess;
	data.reason = buf;
	tree_foreach (sess->usertree, (tree_traverse_func *)mkickops_cb, &data);
	tree_foreach (sess->usertree, (tree_traverse_func *)mkick_cb, &data);

	return TRUE;
}

static int
cmd_devoice (struct session *sess, char *cmd, char *buf)
{
	int i = 1;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	while (1)
	{
		if (!*parv[i])
		{
			if (i == 1)
				return FALSE;
			send_channel_modes (sess, parv, 1, i, '-', 'v');
			return TRUE;
		}
		i++;
	}
}

static int
cmd_discon (struct session *sess, char *cmd, char *buf)
{
	sess->server->disconnect (sess, TRUE, -1);
	return TRUE;
}

static int
cmd_dns (struct session *sess, char *cmd, char *buf)
{
#ifdef WIN32
	PrintText (sess, "DNS is not implemented in Windows.\n");
	return TRUE;
#else
	char *nick;
	struct User *user;
	int parc;
	char *parv[MAX_TOKENS];
	char tbuf[512];

	split_cmd_parv(buf,&parc,parv);

	nick = parv[1];

	if (*nick)
	{
		if (strchr (nick, '.') == NULL)
		{
			user = find_name (sess, nick);
			if (user && user->hostname)
			{
				do_dns (sess, user->nick, user->hostname);
			} else
			{
				sess->server->p_get_ip (sess->server, nick);
				sess->server->doing_dns = TRUE;
			}
		} else
		{
			sprintf (tbuf, "exec -d %s %s", prefs.dnsprogram, nick);
			handle_command (sess, tbuf, FALSE);
		}
		return TRUE;
	}
	return FALSE;
#endif
}

static int
cmd_echo (struct session *sess, char *cmd, char *buf)
{
	split_cmd(&buf); /* Skip the command */
	skip_white(&buf);
	PrintText (sess, buf);
	return TRUE;
}

#ifndef WIN32

static void
exec_check_process (struct session *sess)
{
	int val;

	if (sess->running_exec == NULL)
		return;
	val = waitpid (sess->running_exec->childpid, NULL, WNOHANG);
	if (val == -1 || val > 0)
	{
		close (sess->running_exec->myfd);
		fe_input_remove (sess->running_exec->iotag);
		free (sess->running_exec);
		sess->running_exec = NULL;
	}
}

#ifndef __EMX__
static int
cmd_execs (struct session *sess, char *cmd, char *buf)
{
	int r;

	exec_check_process (sess);
	if (sess->running_exec == NULL)
	{
		EMIT_SIGNAL (XP_TE_NOCHILD, sess, NULL, NULL, NULL, NULL, 0);
		return FALSE;
	}
	r = kill (sess->running_exec->childpid, SIGSTOP);
	if (r == -1)
		PrintText (sess, "Error in kill(2)\n");

	return TRUE;
}

static int
cmd_execc (struct session *sess, char *cmd, char *buf)
{
	int r;

	exec_check_process (sess);
	if (sess->running_exec == NULL)
	{
		EMIT_SIGNAL (XP_TE_NOCHILD, sess, NULL, NULL, NULL, NULL, 0);
		return FALSE;
	}
	r = kill (sess->running_exec->childpid, SIGCONT);
	if (r == -1)
		PrintText (sess, "Error in kill(2)\n");

	return TRUE;
}

static int
cmd_execk (struct session *sess, char *cmd, char *buf)
{
	int r;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	exec_check_process (sess);
	if (sess->running_exec == NULL)
	{
		EMIT_SIGNAL (XP_TE_NOCHILD, sess, NULL, NULL, NULL, NULL, 0);
		return FALSE;
	}
	if (strcmp (parv[1], "-9") == 0)
		r = kill (sess->running_exec->childpid, SIGKILL);
	else
		r = kill (sess->running_exec->childpid, SIGTERM);
	if (r == -1)
		PrintText (sess, "Error in kill(2)\n");

	return TRUE;
}

/* OS/2 Can't have the /EXECW command because it uses pipe(2) not socketpair
   and thus it is simplex --AGL */
static int
cmd_execw (struct session *sess, char *cmd, char *buf)
{
	int len;
	char *temp;

	split_cmd(&buf);
	skip_white(&buf);

	exec_check_process (sess);
	if (sess->running_exec == NULL)
	{
		EMIT_SIGNAL (XP_TE_NOCHILD, sess, NULL, NULL, NULL, NULL, 0);
		return FALSE;
	}
	len = strlen(buf);
	temp = malloc(len + 2);
	sprintf(temp, "%s\n", buf);
	PrintText(sess, temp);
	write(sess->running_exec->myfd, temp, len + 1);
	free(temp);

	return TRUE;
}
#endif /* !__EMX__ */

/* convert ANSI escape color codes to mIRC codes */

static short escconv[] =
/* 0 1 2 3 4 5  6 7  0 1 2 3 4  5  6  7 */
{  1,4,3,5,2,10,6,1, 1,7,9,8,12,11,13,1 };

static void
exec_handle_colors (char *buf, int len)
{
	char numb[16];
	char *nbuf;
	int i = 0, j = 0, k = 0, firstn = 0, col, colf = 0, colb = 0;
	int esc = FALSE, backc = FALSE, bold = FALSE;

	/* any escape codes in this text? */
	if (strchr (buf, 27) == 0)
		return;

	nbuf = malloc (len + 1);

	while (i < len)
	{
		switch (buf[i])
		{
		case '\r':
			break;
		case 27:
			esc = TRUE;
			break;
		case ';':
			if (!esc)
				goto norm;
			backc = TRUE;
			numb[k] = 0;
			firstn = atoi (numb);
			k = 0;
			break;
		case '[':
			if (!esc)
				goto norm;
			break;
		default:
			if (esc)
			{
				if (buf[i] >= 'A' && buf[i] <= 'z')
				{
					if (buf[i] == 'm')
					{
						/* ^[[0m */
						if (k == 0 || (numb[0] == '0' && k == 1))
						{
							nbuf[j] = '\017';
							j++;
							bold = FALSE;
							goto cont;
						}

						numb[k] = 0;
						col = atoi (numb);
						backc = FALSE;

						if (firstn == 1)
							bold = TRUE;

						if (firstn >= 30 && firstn <= 37)
							colf = firstn - 30;

						if (col >= 40)
						{
							colb = col - 40;
							backc = TRUE;
						}

						if (col >= 30 && col <= 37)
							colf = col - 30;

						if (bold)
							colf += 8;

						if (backc)
						{
							colb = escconv[colb % 14];
							colf = escconv[colf % 14];
							j += sprintf (&nbuf[j], "\003%d,%02d", colf, colb);
						} else
						{
							colf = escconv[colf % 14];
							j += sprintf (&nbuf[j], "\003%02d", colf);
						}
					}
cont:				esc = FALSE;
					backc = FALSE;
					k = 0;
				} else
				{
					if (isdigit (buf[i]) && k < (sizeof (numb) - 1))
					{
						numb[k] = buf[i];
						k++;
					}
				}
			} else
			{
norm:			nbuf[j] = buf[i];
				j++;
			}
		}
		i++;
	}

	nbuf[j] = 0;
	memcpy (buf, nbuf, j + 1);
	free (nbuf);
}

#ifndef HAVE_MEMRCHR
static void *
memrchr (const void *block, int c, size_t size)
{
	unsigned char *p;

	for (p = (unsigned char *)block + size; p != block; p--)
		if (*p == c)
			return p;
	return 0;
}
#endif

static gboolean
exec_data (GIOChannel *source, GIOCondition condition, struct nbexec *s)
{
	char *buf, *readpos, *rest;
	int rd, len;
	int sok = s->myfd;

	len = s->buffill;
	if (len) {
		/* append new data to buffered incomplete line */
		buf = malloc(len + 2050);
		memcpy(buf, s->linebuf, len);
		readpos = buf + len;
		free(s->linebuf);
		s->linebuf = NULL;
	}
	else
		readpos = buf = malloc(2050);
	
	rd = read (sok, readpos, 2048);
	if (rd < 1)
	{
		/* The process has died */
		kill(s->childpid, SIGKILL);
		if (len) {
			buf[len] = '\0';
			exec_handle_colors(buf, len);
			if (s->tochannel)
				handle_multiline (s->sess, buf, FALSE, TRUE);
			else
				PrintText (s->sess, buf);
		}
		free(buf);
		waitpid (s->childpid, NULL, 0);
		s->sess->running_exec = NULL;
		fe_input_remove (s->iotag);
		close (sok);
		free (s);
		return TRUE;
	}
	len += rd;
	buf[len] = '\0';

	rest = memrchr(buf, '\n', len);
	if (rest)
		rest++;
	else
		rest = buf;
	if (*rest) {
		s->buffill = len - (rest - buf); /* = strlen(rest) */
		s->linebuf = malloc(s->buffill);
		memcpy(s->linebuf, rest, s->buffill);
		*rest = '\0';
		len -= s->buffill; /* possibly 0 */
	}
	else
		s->buffill = 0;

	if (len) {
		exec_handle_colors (buf, len);
		if (s->tochannel)
			handle_multiline (s->sess, buf, FALSE, TRUE);
		else
			PrintText (s->sess, buf);
	}
	
	free(buf);
	return TRUE;
}

/* needs EOL */
static int
cmd_exec (struct session *sess, char *cmd, char *buf)
{
	int tochannel = FALSE;
	int fds[2], pid = 0;
	struct nbexec *s;
	int shell = TRUE;

	split_cmd(&buf); /* skip the command */
	skip_white(&buf);

	if (!*buf)
		return TRUE;

	exec_check_process (sess);
	if (sess->running_exec != NULL)
	{
		EMIT_SIGNAL (XP_TE_ALREADYPROCESS, sess, NULL, NULL, NULL, NULL, 0);
		return TRUE;
	}

	if (buf[0] == '-')
	{
		char *opt = split_opt(&buf);

		skip_white(&buf);

		if (!opt)
			return FALSE;

		switch(opt[1])
		{
			case 0:
				return FALSE;
			case 'd':
				shell = FALSE;
				break;
			case 'o':
				tochannel = TRUE;
				break;
		}
	}					

	if (shell)
	{
		if (access ("/bin/sh", X_OK) != 0)
		{
			fe_message (_("I need /bin/sh to run!\n"), FALSE);
			return TRUE;
		}
	}

#ifdef __EMX__						  /* if os/2 */
	if (pipe (fds) < 0)
	{
		PrintText (sess, "Pipe create error\n");
		return FALSE;
	}
	setmode (fds[0], O_BINARY);
	setmode (fds[1], O_BINARY);
#else
	if (socketpair (PF_UNIX, SOCK_STREAM, 0, fds) == -1)
	{
		PrintText (sess, "socketpair(2) failed\n");
		return FALSE;
	}
#endif

	s = (struct nbexec *) malloc (sizeof (struct nbexec));
	memset(s, 0, sizeof(*s));
	s->myfd = fds[0];
	s->tochannel = tochannel;
	s->sess = sess;

	pid = fork ();
	if (pid == 0)
	{
		/* This is the child's context */
		close (0);
		close (1);
		close (2);
		/* Close parent's end of pipe */
		close(s->myfd);
		/* Copy the child end of the pipe to stdout and stderr */
		dup2 (fds[1], 1);
		dup2 (fds[1], 2);
		/* Also copy it to stdin so we can write to it */
		dup2 (fds[1], 0);
		/* Now we call /bin/sh to run our cmd ; made it more friendly -DC1 */
		if (shell)
		{
			execl ("/bin/sh", "sh", "-c", buf, 0);
		} else
		{
			char **argv;
			int argc;

			my_poptParseArgvString (cmd, &argc, &argv);
			execvp (argv[0], argv);
		}
		/* not reached unless error */
		/*printf("exec error\n");*/
		fflush (stdout);
		fflush (stdin);
		_exit (0);
	}
	if (pid == -1)
	{
		/* Parent context, fork() failed */

		PrintText (sess, "Error in fork(2)\n");
		close(fds[0]);
		close(fds[1]);
	} else
	{
		/* Parent path */
		close(fds[1]);
		s->childpid = pid;
		s->iotag = fe_input_add (s->myfd, FIA_READ|FIA_EX, exec_data, s);
		sess->running_exec = s;
		return TRUE;
	}
	return FALSE;
}

#endif

static int
cmd_flushq (struct session *sess, char *cmd, char *buf)
{
	char tbuf[512];

	sprintf (tbuf, "Flushing server send queue, %d bytes.\n", sess->server->sendq_len);
	PrintText (sess, tbuf);
	sess->server->flush_queue (sess->server);
	return TRUE;
}

static int
cmd_quit (struct session *sess, char *cmd, char *buf)
{
	split_cmd(&buf);
	skip_white(&buf);

	if (*buf)
		sess->quitreason = buf;

	sess->server->disconnect (sess, TRUE, -1);
	sess->quitreason = NULL;
	return 2;
}

static int
cmd_gate (struct session *sess, char *cmd, char *buf)
{
	char *server_name;
	server *serv = sess->server;

	split_cmd(&buf);
	server_name=split_cmd(&buf);

	if (*server_name)
	{
		char *port = split_cmd(&buf);
#ifdef USE_OPENSSL
		serv->use_ssl = FALSE;
#endif
		server_fill_her_up (serv);
		if (*port)
			serv->connect (serv, server_name, atoi (port), TRUE);
		else
			serv->connect (serv, server_name, 23, TRUE);
		return TRUE;
	}
	return FALSE;
}

typedef struct
{
	char *cmd;
	session *sess;
} getvalinfo;

static void
get_int_cb (int cancel, int val, getvalinfo *info)
{
	char buf[512];

	if (!cancel)
	{
		snprintf (buf, sizeof (buf), "%s %d", info->cmd, val);
		if (is_session (info->sess))
			handle_command (info->sess, buf, FALSE);
	}

	free (info->cmd);
	free (info);
}

static int
cmd_getint (struct session *sess, char *cmd, char *buf)
{
	getvalinfo *info;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	if (!parv[3][0])
		return FALSE;

	info = malloc (sizeof (*info));
	info->cmd = strdup (parv[2]);
	info->sess = sess;

	fe_get_int (parv[3], atoi (parv[1]), get_int_cb, info);

	return TRUE;
}

static void
get_str_cb (int cancel, char *val, getvalinfo *info)
{
	char buf[512];

	if (!cancel)
	{
		snprintf (buf, sizeof (buf), "%s %s", info->cmd, val);
		if (is_session (info->sess))
			handle_command (info->sess, buf, FALSE);
	}

	free (info->cmd);
	free (info);
}

static int
cmd_getstr (struct session *sess, char *cmd, char *buf)
{
	getvalinfo *info;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	if (!parv[3][0])
		return FALSE;

	info = malloc (sizeof (*info));
	info->cmd = strdup (parv[2]);
	info->sess = sess;

	fe_get_str (parv[3], parv[1], get_str_cb, info);

	return TRUE;
}

static int
cmd_gui (struct session *sess, char *cmd, char *buf)
{
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	if (!strcasecmp (parv[1], "HIDE"))
	{
		fe_ctrl_gui (sess, 0, 0);
	} else if (!strcasecmp (parv[1], "SHOW"))
	{
		fe_ctrl_gui (sess, 1, 0);
	} else if (!strcasecmp (parv[1], "FOCUS"))
	{
		fe_ctrl_gui (sess, 2, 0);
	} else if (!strcasecmp (parv[1], "FLASH"))
	{
		fe_ctrl_gui (sess, 3, 0);
	} else if (!strcasecmp (parv[1], "COLOR"))
	{
		fe_ctrl_gui (sess, 4, atoi (parv[2]));
	} else if (!strcasecmp (parv[1], "ICONIFY"))
	{
		fe_ctrl_gui (sess, 5, 0);
	} else
	{
		return FALSE;
	}

	return TRUE;
}

static int
cmd_help (struct session *sess, char *cmd, char *buf)
{
	int longfmt = 0;
	char *helpcmd = "";
	int parc;
	char *parv[MAX_TOKENS];
	char tbuf[512];

	split_cmd_parv(buf,&parc,parv);

	if (tbuf)
		helpcmd = parv[1];
	if (*helpcmd && strcmp (helpcmd, "-l") == 0)
		longfmt = 1;

	if (*helpcmd && !longfmt)
	{
		help (sess, helpcmd, FALSE);
	} else
	{
		struct popup *pop;
		dict_iterator_t it;
		command *cmd;
		char *buf = malloc (4096);
		unsigned int t = 1, j;
		strcpy (buf, _("\nCommands Available:\n\n  "));
		if (longfmt)
		{
			for (it = dict_first(rage_cmd_list); it; it=iter_next(it))
			{
				cmd = (command *)iter_data(it);

				if(!cmd->help || *cmd->help == '\0')
					snprintf (buf, 4096, "   \0034%s\003 :\n", cmd->name);
				else
					snprintf (buf, 4096, "   \0034%s\003 : %s\n", cmd->name,
							 _(cmd->help));
				PrintText (sess, buf);
			}
			buf[0] = 0;
		} else
		{
			for (it = dict_first(rage_cmd_list); it; it=iter_next(it))
			{
				cmd = (command *)iter_data(it);
				strcat (buf, cmd->name);
				t++;
				if (t == 6)
				{
					t = 1;
					strcat (buf, "\n  ");
				} else
					for (j = 0; j < (10 - strlen (cmd->name)); j++)
						strcat (buf, " ");
			}
		}
		 
		strcat (buf,
				  _("\n\nType /HELP <command> for more information, or /HELP -l\n\n"));
		strcat (buf, _("User defined commands:\n\n  "));
		t = 1;
		for (it = dict_first(command_list); it; it=iter_next(it))
		{
			pop = (struct popup *) iter_data(it);
			strcat (buf, pop->name);
			t++;
			if (t == 6)
			{
				t = 1;
				strcat (buf, "\n");
				PrintText (sess, buf);
				strcpy (buf, "  ");
			} else
			{
				if (strlen (pop->name) < 10)
				{
					for (j = 0; j < (10 - strlen (pop->name)); j++)
						strcat (buf, " ");
				}
			}
		}
		strcat (buf, "\n");
		PrintText (sess, buf);
		free (buf);

		PrintText (sess, "\nPlugin defined commands:\n\n");
		plugin_show_help (sess, NULL);

	}
	return TRUE;
}

static int
cmd_ignore (struct session *sess, char *cmd, char *buf)
{
	int i;
	int type = 0;
	int quiet = 0;
	int parc;
	char *parv[MAX_TOKENS];
	char tbuf[512];

	split_cmd_parv(buf,&parc,parv);

	if (!*parv[1])
	{
		ignore_showlist (sess);
		return TRUE;
	}
	if (!*parv[2])
		return FALSE;

	i = 2;
	while (1)
	{
		if (!*parv[i])
		{
			if (type == 0)
				return FALSE;
			i = ignore_add (parv[1], type);
			if (quiet)
				return TRUE;
			switch (i)
			{
			case 1:
				EMIT_SIGNAL (XP_TE_IGNOREADD, sess, parv[1], NULL, NULL, NULL, 0);
				break;
			case 2:	/* old ignore changed */
				EMIT_SIGNAL (XP_TE_IGNORECHANGE, sess, parv[1], NULL, NULL,
								 NULL, 0);
			}
			return TRUE;
		}
		if (!strcasecmp (parv[i], "UNIGNORE"))
			type |= IG_UNIG;
		else if (!strcasecmp (parv[i], "ALL"))
			type |= IG_PRIV | IG_NOTI | IG_CHAN | IG_CTCP | IG_INVI | IG_DCC;
		else if (!strcasecmp (parv[i], "PRIV"))
			type |= IG_PRIV;
		else if (!strcasecmp (parv[i], "NOTI"))
			type |= IG_NOTI;
		else if (!strcasecmp (parv[i], "CHAN"))
			type |= IG_CHAN;
		else if (!strcasecmp (parv[i], "CTCP"))
			type |= IG_CTCP;
		else if (!strcasecmp (parv[i], "INVI"))
			type |= IG_INVI;
		else if (!strcasecmp (parv[i], "QUIET"))
			quiet = 1;
		else if (!strcasecmp (parv[i], "NOSAVE"))
			type |= IG_NOSAVE;
		else if (!strcasecmp (parv[i], "DCC"))
			type |= IG_DCC;
		else
		{
			sprintf (tbuf, _("Unknown arg '%s' ignored."), parv[i]);
			PrintText (sess, tbuf);
		}
		i++;
	}
}

static int
cmd_invite (struct session *sess, char *cmd, char *buf)
{
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	if (!*parv[1])
		return FALSE;
	if (*parv[2])
		sess->server->p_invite (sess->server, parv[2], parv[1]);
	else
		sess->server->p_invite (sess->server, sess->channel, parv[1]);
	return TRUE;
}

static int
cmd_join (struct session *sess, char *cmd, char *buf)
{
	char *chan;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	chan = parv[1];

	if (*chan)
	{
		char *po, *pass = parv[2];
		sess->server->p_join (sess->server, chan, pass);
		if (sess->channel[0] == 0 && sess->waitchannel[0])
		{
			po = strchr (chan, ',');
			if (po)
				*po = 0;
			safe_strcpy (sess->waitchannel, chan, CHANLEN);
		}
		return TRUE;
	}
	return FALSE;
}

static int
cmd_kick (struct session *sess, char *cmd, char *buf)
{
	char *nick;
	char *reason;

	split_cmd(&buf); /* Skip the command */
	nick = split_cmd(&buf);
	skip_white(&buf);
	reason = buf;

	if (*nick)
	{
		sess->server->p_kick (sess->server, sess->channel, nick, reason);
		return TRUE;
	}
	return FALSE;
}

static int
cmd_kickban (struct session *sess, char *cmd, char *buf)
{
	char *nick;
	char *reason;
	struct User *user;
	char tbuf[512];

	split_cmd(&buf); /* Skip the command */
	nick = split_cmd(&buf);
	skip_white(&buf);
	reason = buf;

	if (*nick)
	{
		/* if the reason is a 1 digit number, treat it as a bantype */

		user = find_name (sess, nick);

		if (isdigit (reason[0]) && reason[1] == 0)
		{
			ban (sess, tbuf, nick, reason, (user && user->op));
			reason[0] = 0;
		} else
			ban (sess, tbuf, nick, "", (user && user->op));

		sess->server->p_kick (sess->server, sess->channel, nick, reason);

		return TRUE;
	}
	return FALSE;
}

static int
cmd_killall (struct session *sess, char *cmd, char *buf)
{
	xchat_exit();
	return 2;
}

static int
cmd_lagcheck (struct session *sess, char *cmd, char *buf)
{
	lag_check ();
	return TRUE;
}

static void
lastlog (session *sess, char *search)
{
	session *lastlog_sess;

	if (!is_session (sess))
		return;

	lastlog_sess = find_dialog (sess->server, "(lastlog)");
	if (!lastlog_sess)
		lastlog_sess = new_ircwindow (sess->server, "(lastlog)", SESS_DIALOG, 0);
	lastlog_sess->lastlog_sess = sess;

	fe_text_clear (lastlog_sess);

	fe_lastlog (sess, lastlog_sess, search);
}

static int
cmd_lastlog (struct session *sess, char *cmd, char *buf)
{
	split_cmd(&buf);
	skip_white(&buf);
	if (*buf)
	{
		lastlog (sess, buf);
		return TRUE;
	}

	return FALSE;
}

static int
cmd_list (struct session *sess, char *cmd, char *buf)
{
	split_cmd(&buf);
	skip_white(&buf);
	sess->server->p_list_channels (sess->server, buf);

	return TRUE;
}

/* needs word_eol for arg */
static int
cmd_load (struct session *sess, char *cmd, char *buf)
{
	FILE *fp;
	char *nl, *file;
	char tbuf[1024];

	split_cmd(&buf); /* Skip the command */
	skip_white(&buf);

	if (*buf == '-') {
		char *opt = split_opt(&buf);
		if (strcmp(opt,"-e") == 0) 
		{
			file = expand_homedir (split_cmd(&buf));
			fp = fopen (file, "r");
			free (file);
			if (fp)
			{
				tbuf[1024] = '\0';
				while (fgets (tbuf, 1024, fp))
				{
					nl = strchr (tbuf, '\n');
					if (nl)
						*nl = 0;
					handle_command (sess, tbuf, TRUE);
				}
				fclose (fp);
			}
			return TRUE;
		}
		else
			return FALSE; /* Unknown command */
	}

	file = split_cmd(&buf);

#ifdef USE_PLUGIN
	{
		int len;
		char *error, *arg = NULL;

		len = strlen (file);
#ifdef WIN32
		if (len > 4 && strcasecmp (".dll", file + len - 4) == 0)
#else
#if defined(__hpux)
		if (len > 3 && strcasecmp (".sl", file + len - 3) == 0)
#else
		if (len > 3 && strcasecmp (".so", file + len - 3) == 0)
#endif
#endif
		skip_white(&buf);
		if (buf)
			arg = buf;

		file = expand_homedir (file);
		error = plugin_load (sess, file, arg);
		free (file);

		if (error)
			PrintText (sess, error);

		return TRUE;
	}
#endif

	sprintf (tbuf, "Unknown file type %s. Maybe you need to install the Perl or Python plugin?\n", file);
	PrintText (sess, tbuf);

	return FALSE;
}

static int
cmd_me (struct session *sess, char *cmd, char *buf)
{
	char *act;
	char tbuf[512];

	split_cmd(&buf);
	skip_white(&buf);

	act = buf;

	if (!(*act))
		return FALSE;

	if (sess->type == SESS_SERVER)
	{
		notj_msg (sess);
		return TRUE;
	}

	sprintf (tbuf, "\001ACTION %s\001\r", act);
	/* first try through DCC CHAT */
	if (dcc_write_chat (sess->channel, tbuf))
	{
		/* print it to screen */
		inbound_action (sess, sess->channel, sess->server->nick, act, TRUE);
	} else
	{
		/* DCC CHAT failed, try through server */
		if (sess->server->connected)
		{
			sess->server->p_action (sess->server, sess->channel, act);
			/* print it to screen */
			inbound_action (sess, sess->channel, sess->server->nick, act, TRUE);
		} else
		{
			notc_msg (sess);
		}
	}

	return TRUE;
}

/* TODO: add cmd_umode */
static int
cmd_mode (struct session *sess, char *cmd, char *buf)
{
	char *dest;
	char *mode;

	split_cmd(&buf);
	dest = split_cmd(&buf);
	skip_white(&buf);
	mode = buf;

	/* +channel channels are dying, let those servers whine about modes.
	 * return info about current channel if available and no info is given 
	 * */

	if ((*dest == '+') || (*dest == '-') /* first input is a mode, assume current channel. */
	  ||(*dest == '\0')   /* no argument, get channel information. */
	  || (!is_channel(sess->server, dest) /* not a channel */
           && !(sess->server->p_cmp(sess->server->nick, dest) == 0)))
	{
		if(sess->channel[0] == '\0')
			return FALSE;
		sess->server->p_mode (sess->server, sess->channel, dest);
	}	
	else
		sess->server->p_mode (sess->server, dest, mode);
	return TRUE;
}

static int
mop_cb (struct User *user, multidata *data)
{
	if (!user->op)
	{
		data->nicks[data->i] = user->nick;
		data->i++;
	}
	return TRUE;
}

static int
cmd_mop (struct session *sess, char *cmd, char *buf)
{
	char **nicks = malloc (sizeof (char *) * (sess->total - sess->ops));
	multidata data;

	data.nicks = nicks;
	data.i = 0;
	tree_foreach (sess->usertree, (tree_traverse_func *)mop_cb, &data);
	send_channel_modes (sess, nicks, 0, data.i, '+', 'o');

	free (nicks);

	return TRUE;
}

/* once again, need word_eol[] for msg */
static int
cmd_msg (struct session *sess, char *cmd, char *buf)
{
	char *nick;
	char *msg;
	struct session *newsess;

	split_cmd(&buf);
	nick=split_cmd(&buf);
	skip_white(&buf);
	msg=buf;

	if (*nick)
	{
		if (*msg)
		{
			if (strcmp (nick, ".") == 0)
			{							  /* /msg the last nick /msg'ed */
				if (sess->lastnick[0])
					nick = sess->lastnick;
			} else
			{
				safe_strcpy (sess->lastnick, nick, NICKLEN);	/* prime the last nick memory */
			}

			if (*nick == '=')
			{
				nick++;
				if (!dcc_write_chat (nick, msg))
				{
					EMIT_SIGNAL (XP_TE_NODCC, sess, NULL, NULL, NULL, NULL, 0);
					return TRUE;
				}
			} else
			{
				if (!sess->server->connected)
				{
					notc_msg (sess);
					return TRUE;
				}
				sess->server->p_message (sess->server, nick, msg);
			}
			newsess = find_dialog (sess->server, nick);
			if (!newsess)
				newsess = find_channel (sess->server, nick);
			if (newsess)
				inbound_chanmsg (newsess->server, NULL, newsess->channel,
									  newsess->server->nick, msg, TRUE, FALSE);
			else
			{
				/* mask out passwords */
				/* TODO: do it for X on undernet too */
				if (strcasecmp (nick, "nickserv") == 0 &&
					 strncasecmp (msg, "identify ", 9) == 0)
					msg = "identify ****";
				if (strcasecmp (nick, "authserv") == 0 &&
						strncasecmp (msg, "auth ", 5) == 0)
					msg = "auth ****";
				EMIT_SIGNAL (XP_TE_MSGSEND, sess, nick, msg, NULL, NULL, 0);
			}

			return TRUE;
		}
	}
	return FALSE;
}

static int
cmd_names (struct session *sess, char *cmd, char *buf)
{
	split_cmd(&buf);
	skip_white(&buf);
	if (*buf)
	  	sess->server->p_names (sess->server, buf);
	else
		sess->server->p_names (sess->server, sess->channel);
	return TRUE;
}

static int
cmd_nctcp (struct session *sess, char *cmd, char *buf)
{
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	if (*parv[2])
	{
		sess->server->p_nctcp (sess->server, parv[1], paste_parv(cmd, 512, 2, parc, parv));
		return TRUE;
	}
	return FALSE;
}

static int
cmd_newserver (struct session *sess, char *cmd, char *buf)
{
	sess = new_ircwindow (NULL, NULL, SESS_SERVER, 0);
	cmd_server (sess, cmd, buf);
	return TRUE;
}

static int
cmd_nick (struct session *sess, char *cmd, char *buf)
{
	char *nick;

	split_cmd(&buf);
	skip_white(&buf);

	nick=buf;
	if (*nick)
	{
		if (sess->server->connected)
			sess->server->p_change_nick (sess->server, nick);
		else
			inbound_newnick (sess->server, sess->server->nick, nick, TRUE);
		return TRUE;
	}
	return FALSE;
}

static int
cmd_notice (struct session *sess, char *cmd, char *buf)
{
	char *nick;
	char *msg;

	split_cmd(&buf);
	nick = split_cmd(&buf);
	skip_white(&buf);

	msg=buf;

	if (*nick && *msg)
	{
		sess->server->p_notice (sess->server,nick, msg);
		EMIT_SIGNAL (XP_TE_NOTICESEND, sess, nick, msg, NULL, NULL, 0);
		return TRUE;
	}
	return FALSE;
}

static int
cmd_notify (struct session *sess, char *cmd, char *buf)
{
	int i = 0;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	if (*parv[1])
	{
		while (1)
		{
			i++;
			if (!*parv[i])
				break;
			if (notify_deluser (parv[i]))
			{
				EMIT_SIGNAL (XP_TE_DELNOTIFY, sess, parv[i], NULL, NULL, NULL, 0);
				return TRUE;
			}
			notify_adduser (parv[i]);
			EMIT_SIGNAL (XP_TE_ADDNOTIFY, sess, parv[i], NULL, NULL, NULL, 0);
		}
	} else
		notify_showlist (sess);
	return TRUE;
}

static int
cmd_op (struct session *sess, char *cmd, char *buf)
{
	int i = 1;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	while (1)
	{
		if (!*parv[i])
		{
			if (i == 1)
				return FALSE;
			send_channel_modes (sess, parv, 1, i, '+', 'o');
			return TRUE;
		}
		i++;
	}
}

static int
cmd_part (struct session *sess, char *cmd, char *buf)
{
	char *chan;
	char *reason;

	split_cmd(&buf);

	chan = split_cmd(&buf);
	skip_white(&buf);
	reason = buf;

	if (!*chan)
		chan = sess->channel;
	if ((*chan) && is_channel (sess->server, chan))
	{
		if (reason[0] == 0)
			reason = NULL;
		server_sendpart (sess->server, chan, reason);
		return TRUE;
	}
	return FALSE;
}

static int
cmd_ping (struct session *sess, char *cmd, char *buf)
{
	char timestring[64];
	unsigned long tim;

	split_cmd(&buf);
	skip_white(&buf);

	tim = make_ping_time ();

	snprintf (timestring, sizeof (timestring), "%lu", tim);
	sess->server->p_ping (sess->server, buf, timestring);

	return TRUE;
}

static int

cmd_query (struct session *sess, char *cmd, char *buf)
{
	char *nick;

	split_cmd(&buf);
	skip_white(&buf);
	nick = buf;

	if (*nick && !is_channel (sess->server, nick))
	{
		if (!find_dialog (sess->server, nick))
			new_ircwindow (sess->server, nick, SESS_DIALOG, 1);
		return TRUE;
	}
	return FALSE;
}

static int
cmd_quote (struct session *sess, char *cmd, char *buf)
{
	split_cmd(&buf);
	skip_white(&buf);

	return sess->server->p_raw (sess->server, buf);
}

static int
cmd_reconnect (struct session *sess, char *cmd, char *buf)
{
	int tmp = prefs.recon_delay;
	GSList *list;
	server *serv = sess->server;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	prefs.recon_delay = 0;

	if (!strcasecmp (parv[1], "ALL"))
	{
		list = serv_list;
		while (list)
		{
			serv = list->data;
			if (serv->connected)
				serv->auto_reconnect (serv, TRUE, -1);
			list = list->next;
		}
	}	
	/* If it isn't "ALL" and there is something 
	there it *should* be a server they are trying to connect to*/
	else if (*parv[1])
	{
		int offset = 0;
#ifdef USE_OPENSSL
		int use_ssl = FALSE;

		if (strcmp (parv[1], "-ssl") == 0)
		{
			use_ssl = TRUE;
			offset++;	/* args move up by 1 word */
		}
		serv->use_ssl = use_ssl;
		serv->accept_invalid_cert = TRUE;
#endif

		if (*parv[3+offset])
			safe_strcpy (serv->password, parv[3+offset], sizeof (serv->password));
		if (*parv[2+offset])
			serv->port = atoi (parv[2+offset]);
		safe_strcpy (serv->hostname, parv[1+offset], sizeof (serv->hostname));
		serv->auto_reconnect (serv, TRUE, -1);
	}
	else
	{
		serv->auto_reconnect (serv, TRUE, -1);
	}	
	prefs.recon_delay = tmp;

	return TRUE;
}

static int
cmd_recv (struct session *sess, char *msg, char *buf)
{
	split_cmd(&buf);
	skip_white(&buf);
	if (*buf)
	{
		sess->server->p_inline (sess->server, buf, (int)strlen (buf));
		return TRUE;
	}

	return FALSE;
}

static int
cmd_rping (struct session *sess, char *msg, char *buf)
{
	char *parv[MAX_TOKENS];
	int parc;
	time_t tp;
	char line[512];

	split_cmd_parv(buf,&parc,parv);

	if (parc == 0)
		return FALSE;

	tp = time(NULL);

	if (parc > 1)
		snprintf(line, 512, "RPING %s %s :%li\r\n", parv[1], parv[2], tp);
	else
		snprintf(line, 512, "RPING %s %s :%li\r\n", sess->server->servername,
				parv[1], tp);
	
	sess->server->p_raw (sess->server, line);
	
	return TRUE;
}

static int
cmd_say (struct session *sess, char *cmd, char *buf)
{
	split_cmd(&buf);
	skip_white(&buf);
	if (*buf)
	{
		handle_say (sess, buf, FALSE);
		return TRUE;
	}
	return FALSE;
}

static int
cmd_settab (struct session *sess, char *cmd, char *buf)
{
	split_cmd(&buf);
	skip_white(&buf);
	if (*buf)
	{
		char tbuf[1024];
		strcpy (tbuf, sess->channel);
		safe_strcpy (sess->channel, buf, CHANLEN);
		fe_set_channel (sess);
		strcpy (sess->channel, tbuf);
	}

	return TRUE;
}

static int
cmd_server (struct session *sess, char *cmd, char *buf)
{
	int offset = 0;
	char *server_name;
	char *port;
	char *pass;
	char *co;
	server *serv = sess->server;
	int use_ssl = FALSE;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

#ifdef USE_OPENSSL

	/* BitchX uses -ssl, mIRC uses -e, let's support both */
	if (strcmp (parv[1], "-ssl") == 0 || strcmp (parv[1], "-e") == 0)
	{
		use_ssl = TRUE;
		offset++;	/* args move up by 1 word */
	}
#endif

	server_name = parv[1 + offset];
	port = parv[2 + offset];
	pass = parv[3 + offset];

	if (!(*server_name))
		return FALSE;

	sess->server->network = NULL;

	/* dont clear it for /servchan */
	if (strncasecmp (parv[0], "SERVCHAN ", 9))
		sess->willjoinchannel[0] = 0;

#ifdef USE_OPENSSL
	if (strncasecmp ("ircs://", server_name, 7) == 0)
	{
		use_ssl = TRUE;
		server_name += 7;
		goto urlserv;
	}
#endif

	if (strncasecmp ("irc://", server_name, 6) == 0)
	{
		server_name += 6;
#ifdef USE_OPENSSL
urlserv:
#endif
		/* check for port */
		co = strchr (server_name, ':');
		if (co)
		{
			port = co + 1;
			*co = 0;
			pass = parv[2 + offset];
		} else
			co = server_name;
		/* check for channel - mirc style */
		co = strchr (co + 1, '/');
		if (co)
		{
			*co = 0;
			co++;
			if (*co == '#')
			{
				safe_strcpy (sess->willjoinchannel, co, CHANLEN);
			} else
			{
				sess->willjoinchannel[0] = '#';
				safe_strcpy ((sess->willjoinchannel + 1), co, (CHANLEN - 1));
			}
		}
	}

	/* support +7000 style ports like mIRC */
	if (port[0] == '+')
	{
		port++;
#ifdef USE_OPENSSL
		use_ssl = TRUE;
#endif
	}

	if (*pass)
	{
		safe_strcpy (serv->password, pass, sizeof (serv->password));
	}
#ifdef USE_OPENSSL
	serv->use_ssl = use_ssl;
	serv->accept_invalid_cert = TRUE;
#endif

	/* try to connect by Network name */
	if (servlist_connect_by_netname (sess, server_name))
		return TRUE;

	if (*port)
	{
		serv->connect (serv, server_name, atoi (port), FALSE);
	} else
	{
		/* -1 for default port */
		serv->connect (serv, server_name, -1, FALSE);
	}
	return TRUE;
}

/* TODO: /server supports -e, servchan doesn't */
static int
cmd_servchan (struct session *sess, char *cmd, char *buf)
{
	int offset = 0;
	char *tmp;
	int parc;
	char *parv[MAX_TOKENS];

	tmp=strdup(buf);
	split_cmd_parv(buf,&parc,parv);

#ifdef USE_OPENSSL
	if (strcmp (parv[1], "-ssl") == 0)
		offset++;
#endif

	if (*parv[3 + offset])
	{
		safe_strcpy (sess->willjoinchannel, parv[3 + offset], CHANLEN);
		free(tmp);
		return cmd_server (sess, cmd, buf);
	}

	free(tmp);
	return FALSE;
}

static int
cmd_topic (struct session *sess, char *cmd, char *buf)
{
	char *p;

	split_cmd(&buf);

	/* Is there a space in the sentence?
	 * if there is lets add a seperator.
	 */
	if((p = strchr(buf, ' ')))
		*p++ = 0;

	/* Now, is the first word of the seperated
	 * sentence a channel? =)
	 */
	if (p && is_channel (sess->server, buf))
		sess->server->p_topic (sess->server, buf, p);
	else 
	{
		/* Replace the missing space if removed */
		if (p)
			*--p = ' ';
		sess->server->p_topic (sess->server, sess->channel, buf);
	}

	return TRUE;
}

static int
cmd_unignore (struct session *sess, char *cmd, char *buf)
{
	char *mask;
	char *arg;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	mask=parv[1];
	arg=parv[2];
	
	if (*mask)
	{
		if (ignore_del (mask, NULL))
		{
			if (strcasecmp (arg, "QUIET"))
				EMIT_SIGNAL (XP_TE_IGNOREREMOVE, sess, mask, NULL, NULL, NULL, 0);
		}
		return TRUE;
	}
	return FALSE;
}

static int
cmd_unload (struct session *sess, char *cmd, char *buf)
{
#ifdef USE_PLUGIN
	int len, by_file = FALSE;

	split_cmd(&buf);
	skip_white(&buf);

	len = strlen (buf);
#ifdef WIN32
	if (len > 4 && strcasecmp (buf + len - 4, ".dll") == 0)
#else
#if defined(__hpux)
	if (len > 3 && strcasecmp (buf + len - 3, ".sl") == 0)
#else
	if (len > 3 && strcasecmp (buf + len - 3, ".so") == 0)
#endif
#endif
		by_file = TRUE;

	switch (plugin_kill (buf, by_file))
	{
	case 0:
			PrintText (sess, _("No such plugin found.\n"));
			break;
	case 1:
			return TRUE;
	case 2:
			PrintText (sess, _("That plugin is refusing to unload.\n"));
			break;
	}
#endif

	return FALSE;
}

static int
userlist_cb (struct User *user, session *sess)
{
	time_t lt;

	if (!user->lasttalk)
		lt = 0;
	else
		lt = time (0) - user->lasttalk;
	PrintTextf (sess,
				"\00306%s\t\00314[\00310%-38s\00314] \017ov\0033=\017%d%d away=%u lt\0033=\017%d\n",
				user->nick, user->hostname, user->op, user->voice, user->away, lt);

	return TRUE;
}

static int
cmd_userlist (struct session *sess, char *cmd, char *buf)
{
	tree_foreach (sess->usertree, (tree_traverse_func *)userlist_cb, sess);
	return TRUE;
}

static int
wallchop_cb (struct User *user, multidata *data)
{
	if (user->op)
	{
		if (data->i)
			strcat (data->tbuf, ",");
		strcat (data->tbuf, user->nick);
		data->i++;
	}
	if (data->i == 5)
	{
		data->i = 0;
		sprintf (data->tbuf + strlen (data->tbuf),
					" :[@%s] %s", data->sess->channel, data->reason);
		data->sess->server->p_raw (data->sess->server, data->tbuf);
		strcpy (data->tbuf, "NOTICE ");
	}

	return TRUE;
}

static int
wallvoices_cb (struct User *user, multidata *data)
{
	if (user->voice)
	{
		if (data->i)
			strcat (data->tbuf, ",");
		strcat (data->tbuf, user->nick);
		data->i++;
	}
	if (data->i == 5)
	{
		data->i = 0;
		sprintf (data->tbuf + strlen (data->tbuf),
				" :[@%s] %s", data->sess->channel, data->reason);
		data->sess->server->p_raw (data->sess->server, data->tbuf);
		strcpy (data->tbuf, "NOTICE ");
	}

	return TRUE;
}

static int
cmd_wallchops (struct session *sess, char *cmd, char *buf)
{
	multidata data;
	char sent[1024];

	split_cmd(&buf);
	skip_white(&buf);

	if (!(*buf))
		return FALSE;

	if (isupport(sess->server, "WALLCHOPS"))
	{
		sprintf(sent,"WALLCHOPS %s :[@%s] %s",
				sess->channel,
				sess->channel,
				buf);
		sess->server->p_raw (sess->server, sent);
		sprintf(sent,"[@%s] %s", sess->channel,	buf);
		EMIT_SIGNAL (XP_TE_NOTICE, sess, sess->me->nick, sent, NULL, NULL, 0);
		return TRUE;
	} else 
	{
		strcpy (sent, "NOTICE ");

		data.reason = buf;
		data.tbuf = sent;
		data.i = 0;
		data.sess = sess;
		tree_foreach (sess->usertree, (tree_traverse_func*)wallchop_cb, &data);

		if (data.i)
		{
			sprintf (sent + strlen (sent),
						" :[@%s] %s", sess->channel, buf);
			sess->server->p_raw (sess->server, sent);
		}
	}
	return TRUE;
}

static int
cmd_wallvoices (struct session *sess, char *cmd, char *buf)
{
	multidata data;
	char sent[1024];

	split_cmd(&buf);
	skip_white(&buf);

	if (!(*buf))
		return FALSE;

	if (isupport(sess->server, "WALLVOICES"))
	{
		sprintf(sent,"WALLVOICES %s :[+@%s] %s",
				sess->channel,
				sess->channel,
				buf);
		sess->server->p_raw (sess->server, sent);
		sprintf(sent,"[+@%s] %s", sess->channel, buf);
		EMIT_SIGNAL (XP_TE_NOTICE, sess, sess->me->nick, sent, NULL, NULL, 0);
		return TRUE;
	} else
	{
		strcpy (sent, "NOTICE ");

		data.reason = buf;
		data.tbuf = sent;
		data.i = 0;
		data.sess = sess;
		tree_foreach (sess->usertree, (tree_traverse_func*)wallvoices_cb, &data);

		if (data.i)
		{
			sprintf (sent + strlen (sent),
					" :[@%s] %s", sess->channel, buf);
			sess->server->p_raw (sess->server, sent);
		}
	}
	return TRUE;
}

static int
cmd_wallchan (struct session *sess, char *cmd, char *buf)
{
	GSList *list;

	split_cmd(&buf);
	skip_white(&buf);

	if (*buf)
	{
		list = sess_list;
		while (list)
		{
			sess = list->data;
			if (sess->type == SESS_CHANNEL)
			{
				inbound_chanmsg (sess->server, NULL, sess->channel,
									  sess->server->nick, buf, TRUE, FALSE);
				sess->server->p_message (sess->server, sess->channel, buf);
			}
			list = list->next;
		}
		return TRUE;
	}
	return FALSE;
}

static int
cmd_hop (struct session *sess, char *cmd, char *buf)
{
	int i = 1;
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	while (1)
	{
		if (!*parv[i])
		{
			if (i == 1)
				return FALSE;
			send_channel_modes (sess, parv, 1, i, '+', 'h');
			return TRUE;
		}
		i++;
	}
}

static int
cmd_voice (struct session *sess, char *cmd, char *buf)
{
	int parc;
	char *parv[MAX_TOKENS];
	int i = 1;

	split_cmd_parv(buf,&parc,parv);

	while (1)
	{
		if (!*parv[i])
		{
			if (i == 1)
				return FALSE;
			send_channel_modes (sess, parv, 1, i, '+', 'v');
			return TRUE;
		}
		i++;
	}
}

static void
add_command(command *cmd)
{
	dict_insert(rage_cmd_list, cmd->name, cmd);
}

int
add_plugin_command(command *cmd)
{
	int found;

	dict_find(rage_cmd_list, cmd->name, &found);
	if (!found)
		dict_insert(rage_cmd_list, cmd->name, cmd);
	return !found;
}

GList *
get_command_list(GList *list)
{
	dict_iterator_t it;
	for (it=dict_first(command_list); it; it=iter_next(it))
		list = g_list_prepend (list, (char *)iter_key(it));
	for (it=dict_first(rage_cmd_list); it; it=iter_next(it))
		list = g_list_prepend (list, (char *)iter_key(it));
	return list;
}

static command commands[] = {
	{"ADDBUTTON", cmd_addbutton, 0, 0,
	N_("ADDBUTTON <name> <action>, adds a button under the user-list")},
	{"ALLCHAN", cmd_allchannels, 0, 0,
	 N_("ALLCHAN <cmd>, sends a command to all channels you're in")},
	{"ALLSERV", cmd_allservers, 0, 0,
	 N_("ALLSERV <cmd>, sends a command to all servers you're in")},
	{"AWAY", cmd_away, 1, 0, N_("AWAY [<reason>], sets you away")},
	{"BAN", cmd_ban, 1, 1,
	 N_("BAN <mask> [<bantype>], bans everyone matching the mask from the current channel. If they are already on the channel this doesn't kick them (needs chanop)")},
	{"CHARSET", cmd_charset, 0, 0, 0},
	{"CLEAR", cmd_clear, 0, 0, N_("CLEAR, Clears the current text window")},
	{"CLOSE", cmd_close, 0, 0, N_("CLOSE, Closes the current window/tab")},
	{"COUNTRY", cmd_country, 0, 0,
	 N_("COUNTRY <code>, finds a country code, eg: au = australia")},
	{"CTCP", cmd_ctcp, 1, 0,
	 N_("CTCP <nick> <message>, send the CTCP message to nick, common messages are VERSION and USERINFO")},
	{"CYCLE", cmd_cycle, 1, 1,
	N_("CYCLE, parts current channel and immediately rejoins")},
	{"DCC", cmd_dcc, 0, 0,
	 N_("\n"
	 "DCC GET <nick>                     - accept an offered file\n"
	 "DCC SEND [-maxcps=#] <nick> [file] - send a file to someone\n"
	 "DCC LIST                           - show DCC list\n"
	 "DCC CHAT <nick>                    - offer DCC CHAT to someone\n"
	 "DCC CLOSE <type> <nick> <file>         example:\n"
	 "         /dcc close send johnsmith file.tar.gz")},
	{"DEBUG", cmd_debug, 0, 0, 0},
	{"DEHOP", cmd_dehop, 1, 1,
	 N_("DEHOP <nick>, removes chanhalf-op status from the nick on the current channel (needs chanop)")},
	{"DELBUTTON", cmd_delbutton, 0, 0,
	 N_("DELBUTTON <name>, deletes a button from under the user-list")},
	{"DEOP", cmd_deop, 1, 1,
	 N_("DEOP <nick>, removes chanop status from the nick on the current channel (needs chanop)")},
	{"DEVOICE", cmd_devoice, 1, 1,
	 N_("DEVOICE <nick>, removes voice status from the nick on the current channel (needs chanop)")},
	{"DISCON", cmd_discon, 0, 0, N_("DISCON, Disconnects from server")},
	{"HOST", cmd_dns, 0, 0, N_("DNS <nick|host|ip>, Finds a users IP number")},
	{"ECHO", cmd_echo, 0, 0, N_("ECHO <text>, Prints text locally")},
#ifndef WIN32
	{"EXEC", cmd_exec, 0, 0,
	 N_("EXEC [-o] <command>, runs the command. If -o flag is used then output is sent to current channel, else is printed to current text box")},
#ifndef __EMX__
	{"EXECCONT", cmd_execc, 0, 0, N_("EXECCONT, sends the process SIGCONT")},
#endif
	{"EXECKILL", cmd_execk, 0, 0,
	 N_("EXECKILL [-9], kills a running exec in the current session. If -9 is given the process is SIGKILL'ed")},
#ifndef __EMX__
	{"EXECSTOP", cmd_execs, 0, 0, N_("EXECSTOP, sends the process SIGSTOP")},
	{"EXECWRITE", cmd_execw, 0, 0, N_("EXECWRITE, sends data to the processes stdin")},
#endif /* __EMX__ */
#endif /* WIN32 */
	{"FLUSHQ", cmd_flushq, 0, 0,
	 N_("FLUSHQ, flushes the current server's send queue")},
	{"GATE", cmd_gate, 0, 0,
	N_("GATE <host> [<port>], proxies through a host, port defaults to 23")},
	{"GETINT", cmd_getint, 0, 0, "GETINT <default> <command> <prompt>"},
	{"GETSTR", cmd_getstr, 0, 0, "GETSTR <default> <command> <prompt>"},
	{"GUI", cmd_gui, 0, 0, "GUI [SHOW|HIDE|FOCUS|FLASH|ICONIFY|COLOR <n>]"},
	{"HELP", cmd_help, 0, 0, 0},
	{"HOP", cmd_hop, 1, 1,
	 N_("HOP <nick>, gives chanhalf-op status to the nick (needs chanop)")},
	{"IGNORE", cmd_ignore, 0, 0,
	 N_("IGNORE <mask> <types..> <options..>\n"
	 "    mask - host mask to ignore, eg: *!*@*.aol.com\n"
	 "    types - types of data to ignore, one or all of:\n"
	 "            PRIV, CHAN, NOTI, CTCP, DCC, INVI, ALL\n"
	 "    options - NOSAVE, QUIET")},
	{"INVITE", cmd_invite, 1, 0,
	 N_("INVITE <nick> [<channel>], invites someone to a channel, by default the current channel (needs chanop)")},
	{"JOIN", cmd_join, 1, 0, N_("JOIN <channel>, joins the channel")},
	{"KICK", cmd_kick, 1, 1,
	 N_("KICK <nick>, kicks the nick from the current channel (needs chanop)")},
	{"KICKBAN", cmd_kickban, 1, 1,
	 N_("KICKBAN <nick>, bans then kicks the nick from the current channel (needs chanop)")},
	{"KILLALL", cmd_killall, 0, 0, "KILLALL, immediately exit"},
	{"LAGCHECK", cmd_lagcheck, 0, 0,
	 N_("LAGCHECK, forces a new lag check")},
	{"LASTLOG", cmd_lastlog, 0, 0,
	 N_("LASTLOG <string>, searches for a string in the buffer")},
	{"LIST", cmd_list, 1, 0, 0},
	{"LOAD", cmd_load, 0, 0, N_("LOAD <file>, loads a plugin or script")},
	{"MDEHOP", cmd_mdehop, 1, 1,
	 N_("MDEHOP, Mass deop's all chanhalf-ops in the current channel (needs chanop)")},
	{"MDEOP", cmd_mdeop, 1, 1,
	 N_("MDEOP, Mass deop's all chanops in the current channel (needs chanop)")},
	{"ME", cmd_me, 0, 0,
	 N_("ME <action>, sends the action to the current channel (actions are written in the 3rd person, like /me jumps)")},
	{"MKICK", cmd_mkick, 1, 1,
	 N_("MKICK, Mass kicks everyone except you in the current channel (needs chanop)")},
	{"MODE", cmd_mode, 1, 0, 0},
	{"MOP", cmd_mop, 1, 1,
	 N_("MOP, Mass op's all users in the current channel (needs chanop)")},
	{"MSG", cmd_msg, 0, 0, N_("MSG <nick> <message>, sends a private message")},
	{"NAMES", cmd_names, 1, 0,
	 N_("NAMES, Lists the nicks on the current channel")},
	{"NCTCP", cmd_nctcp, 1, 0,
	 N_("NCTCP <nick> <message>, Sends a CTCP notice")},
	{"NEWSERVER", cmd_newserver, 0, 0, N_("NEWSERVER <hostname> [<port>]")},
	{"NICK", cmd_nick, 0, 0, N_("NICK <nickname>, sets your nick")},
	{"NOTICE", cmd_notice, 1, 0,
	 N_("NOTICE <nick/channel> <message>, sends a notice. Notices are a type of message that should be auto reacted to")},
	{"NOTIFY", cmd_notify, 0, 0,
	 N_("NOTIFY [<nick>], lists your notify list or adds someone to it")},
	{"OP", cmd_op, 1, 1,
	 N_("OP <nick>, gives chanop status to the nick (needs chanop)")},
	{"PART", cmd_part, 1, 1,
	 N_("PART [<channel>] [<reason>], leaves the channel, by default the current one")},
	{"PING", cmd_ping, 1, 0,
	 N_("PING <nick | channel>, CTCP pings nick or channel")},
	{"QUERY", cmd_query, 0, 0,
	 N_("QUERY <nick>, opens up a new privmsg window to someone")},
	{"QUIT", cmd_quit, 0, 0,
	 N_("QUIT [<reason>], disconnects from the current server")},
	{"QUOTE", cmd_quote, 1, 0,
	 N_("QUOTE <text>, sends the text in raw form to the server")},
#ifdef USE_OPENSSL
	{"RECONNECT", cmd_reconnect, 0, 0,
	 N_("RECONNECT [-ssl] [<host>] [<port>] [<password>], Can be called just as /RECONNECT to reconnect to the current server or with /RECONNECT ALL to reconnect to all the open servers")},
#else 
	{"RECONNECT", cmd_reconnect, 0, 0,
	 N_("RECONNECT [<host>] [<port>] [<password>], Can be called just as /RECONNECT to reconnect to the current server or with /RECONNECT ALL to reconnect to all the open servers")},
#endif
	{"RECV", cmd_recv, 1, 0, N_("RECV <text>, send raw data to xchat, as if it was received from the irc server")},
	{"RPING", cmd_rping, 1, 0, N_("RPING <server> [server], ping between servers")},
	{"SAY", cmd_say, 0, 0,
	 N_("SAY <text>, sends the text to the object in the current window")},
#ifdef USE_OPENSSL
	{"SERVCHAN", cmd_servchan, 0, 0,
	 N_("SERVCHAN [-ssl] <host> <port> <channel>, connects and joins a channel")},
#else
	{"SERVCHAN", cmd_servchan, 0, 0,
	 N_("SERVCHAN <host> <port> <channel>, connects and joins a channel")},
#endif

#ifdef USE_OPENSSL
	{"SERVER", cmd_server, 0, 0,
	 N_("SERVER [-ssl] <host> [<port>] [<password>], connects to a server, the default port is 6667 for normal connections, and 9999 for ssl connections")},
#else
	{"SERVER", cmd_server, 0, 0,
	 N_("SERVER <host> [<port>] [<password>], connects to a server, the default port is 6667")},
#endif
	{"SET", cmd_set, 0, 0, N_("SET [-quiet] <variable> [<value>]")},
	{"SETTAB", cmd_settab, 0, 0, 0},
	{"TOPIC", cmd_topic, 1, 1,
	 N_("TOPIC [<topic>], sets the topic if one is given, else shows the current topic")},
	{"UNBAN", cmd_unban, 1, 1,
	 N_("UNBAN <mask> [<mask>...], unbans the specified masks.")},
	{"UNIGNORE", cmd_unignore, 0, 0, N_("UNIGNORE <mask> [QUIET]")},
	{"UNLOAD", cmd_unload, 0, 0, N_("UNLOAD <name>, unloads a plugin or script")},
	{"USERLIST", cmd_userlist, 1, 1, 0},
	{"VOICE", cmd_voice, 1, 1,
	 N_("VOICE <nick>, gives voice status to someone (needs chanop)")},
	{"WALLCHAN", cmd_wallchan, 1, 1,
	 N_("WALLCHAN <message>, writes the message to all channels")},
	{"WALLCHOPS", cmd_wallchops, 1, 1,
	 N_("WALLCHOPS <message>, sends the message to all chanops on the current channel")},
	{"WALLVOICES", cmd_wallvoices, 1, 1,
	N_("WALLVOICES <message>, sends the message to all voiced users on the current channel")},
};

void
setup_commands(void)
{
	int i;
	int nbr_commands = sizeof (commands) / sizeof (command);

	rage_cmd_list = dict_new();

	for (i = 0; i < nbr_commands; i++)
		add_command(&commands[i]);
}


static command *
find_internal_command (char *name)
{
	int present;
	return dict_find(rage_cmd_list, name, &present);
}

static struct popup *
find_user_command (char *name)
{
	int present;
	return dict_find(command_list, name, &present);
}

static void
help (session *sess, char *helpcmd, int quiet)
{
	command *cmd;
	char tbuf[512];

	if (plugin_show_help (sess, helpcmd))
		return;

	cmd = find_internal_command (helpcmd);

	if (cmd)
	{
		if (cmd->help)
		{
			sprintf (tbuf, _("Usage: %s\n"), _(cmd->help));
			PrintText (sess, tbuf);
		} else
		{
			if (!quiet)
				PrintText (sess, _("\nNo help available on that command.\n"));
		}
		return;
	}

	if (!quiet)
		PrintText (sess, _("No such command.\n"));
}

/* auto_insert:
 * dest: destination to write into
 * destlen: length of dest buffer
 * src: source format string
 * parc: number of parv fields
 * parv: parameter vector
 * a: 
 * c: channel?
 * d: 
 * h: 
 * n: netname
 * s: server
 *
 * inserts %a, %c, %d etc into buffer. 
 *   returns 2 on buffer overflow
 *   returns 1 on success                                                   
 *   returns 0 on bad-args-for-user-command                                
 * - word/word_eol args might be NULL                                     
 * - this beast is used for UserCommands, UserlistButtons and CTCP replies
 *
 * supported expandos:
 *  %<num> 	parv[num] where num<100
 *  %<num> 	Unicode charactor num where num>=100
 *  %% 		literal %
 *  %a		argument a
 *  %c		channel
 *  %d		argument d
 *  %h		argumet h
 *  %m		CPU string
 *  %n		nick
 *  %s		server
 *  %t		time in unix seconds
 *  %v		version string
 *  %y		date in YYYYmmdd
 *
 *  $t 		literal tab
 *
 *  &<num> 	parv[num...]
 */

int
auto_insert (char *dest, int destlen, unsigned char *src, int parc, char *parv[],
				 char *a, char *c, char *d, char *h, char *n,
				 char *s)
{
	int num;
	char buf[32];
	time_t now;
	struct tm *tm_ptr;
	char *utf;
	gsize utf_len;
	char *orig = dest;

	destlen--;

	while (src[0])
	{
		if (src[0] == '%' || src[0] == '&')
		{
			if (isdigit (src[1]))
			{
				if (isdigit (src[2]) && isdigit (src[3]))
				{
					buf[0] = src[1];
					buf[1] = src[2];
					buf[2] = src[3];
					buf[3] = 0;
					dest[0] = atoi (buf);
					utf = g_locale_to_utf8 (dest, 1, 0, &utf_len, 0);
					if (utf)
					{
						if ((dest - orig) + utf_len >= (unsigned)destlen)
						{
							g_free (utf);
							return 2;
						}

						memcpy (dest, utf, utf_len);
						g_free (utf);
						dest += utf_len;
					}
					src += 3;
				} else
				{
					if (parv)
					{
						src++;
						num = src[0] - '0';	/* ascii to decimal */
						if (*parv[num] == 0)
							return 0;

						utf = parv[num];

						/* avoid recusive usercommand overflow */
						if ((dest - orig) + strlen (utf) >= (unsigned)destlen)
							return 2;

						strcpy (dest, utf);
						dest += strlen (dest);
					}
				}
			} else
			{
				if (src[0] == '&')
					goto lamecode;
				src++;
				utf = NULL;
				switch (src[0])
				{
				case '%':
					if ((dest - orig) + 2 >= destlen)
						return 2;
					dest[0] = '%';
					dest[1] = 0;
					break;
				case 'a':
					utf = a; break;
				case 'c':
					utf = c; break;
				case 'd':
					utf = d; break;
				case 'h':
					utf = h; break;
				case 'm':
					utf = get_cpu_str (); break;
				case 'n':
					utf = n; break;
				case 's':
					utf = s; break;
				case 't':
					now = time (0);
					utf = ctime (&now);
					utf[19] = 0;
					break;
				case 'v':
					utf = VERSION; break;
					break;
				case 'y':
					now = time (0);
					tm_ptr = localtime (&now);
					snprintf (buf, sizeof (buf), "%4d%02d%02d", 1900 +
								 tm_ptr->tm_year, 1 + tm_ptr->tm_mon, tm_ptr->tm_mday);
					utf = buf;
					break;
				default:
					src--;
					goto lamecode;
				}

				if (utf)
				{
					if ((dest - orig) + strlen (utf) >= (unsigned)destlen)
						return 2;
					strcpy (dest, utf);
					dest += strlen (dest);
				}

			}
			src++;
		} else
		{
			utf_len = g_utf8_skip[src[0]];

			if ((dest - orig) + utf_len >= (unsigned)destlen)
				return 2;

			if (utf_len == 1)
			{
		 lamecode:
				dest[0] = src[0];
				dest++;
				src++;
			} else
			{
				memcpy (dest, src, utf_len);
				dest += utf_len;
				src += utf_len;
			}
		}
	}

	dest[0] = 0;

	return 1;
}

void
check_special_chars (char *cmd, int do_ascii) /* check for %X */
{
	int occur = 0;
	size_t len = strlen (cmd);
	char *buf, *utf;
	char tbuf[4];
	unsigned int i = 0, j = 0;
	gsize utf_len;

	if (!len)
		return;

	buf = malloc (len + 1);

	if (buf)
	{
		while (cmd[j])
		{
			switch (cmd[j])
			{
			case '%':
				occur++;
				if (	do_ascii &&
						j + 3 < len &&
						(isdigit (cmd[j + 1]) && isdigit (cmd[j + 2]) &&
						isdigit (cmd[j + 3])))
				{
					tbuf[0] = cmd[j + 1];
					tbuf[1] = cmd[j + 2];
					tbuf[2] = cmd[j + 3];
					tbuf[3] = 0;
					buf[i] = atoi (tbuf);
					utf = g_locale_to_utf8 (buf + i, 1, 0, &utf_len, 0);
					if (utf)
					{
						memcpy (buf + i, utf, utf_len);
						g_free (utf);
						i += (utf_len - 1);
					}
					j += 3;
				} else
				{
					switch (cmd[j + 1])
					{
					case 'R':
						buf[i] = '\026';
						break;
					case 'U':
						buf[i] = '\037';
						break;
					case 'B':
						buf[i] = '\002';
						break;
					case 'C':
						buf[i] = '\003';
						break;
					case 'O':
						buf[i] = '\017';
						break;
					case '%':
						buf[i] = '%';
						break;
					default:
						buf[i] = '%';
						j--;
						break;
					}
					j++;
					break;
			default:
					buf[i] = cmd[j];
				}
			}
			j++;
			i++;
		}
		buf[i] = 0;
		if (occur)
			strcpy (cmd, buf);
		free (buf);
	}
}

typedef struct
{
	char *nick;
	int len;
	struct User *best;
	int bestlen;
	char *space;
	char *tbuf;
} nickdata;

static int
nick_comp_cb (struct User *user, nickdata *data)
{
	size_t lenu;

	if (!rfc_ncasecmp (user->nick, data->nick, data->len))
	{
		lenu = strlen (user->nick);
		if (lenu == data->len)
		{
			sprintf (data->tbuf, "%s%s", user->nick, data->space);
			data->len = -1;
			return FALSE;
		} else if (lenu < (unsigned)data->bestlen)
		{
			data->bestlen = (int)lenu;
			data->best = user;
		}
	}

	return TRUE;
}

static void
perform_nick_completion (struct session *sess, char *cmd, char *tbuf)
{
	size_t len;
	char *space = strchr (cmd, ' ');
	if (space && space != cmd)
	{
		if (space[-1] == prefs.nick_suffix[0] && space - 1 != cmd)
		{
			len = space - cmd - 1;
			if (len < NICKLEN)
			{
				char nick[NICKLEN];
				nickdata data;

				memcpy (nick, cmd, len);
				nick[len] = 0;

				data.nick = nick;
				data.len = (int)len;
				data.bestlen = INT_MAX;
				data.best = NULL;
				data.tbuf = tbuf;
				data.space = space - 1;
				tree_foreach (sess->usertree, (tree_traverse_func *)nick_comp_cb, &data);

				if (data.len == -1)
					return;

				if (data.best)
				{
					sprintf (tbuf, "%s%s", data.best->nick, space - 1);
					return;
				}
			}
		}
	}

	strcpy (tbuf, cmd);
}

static void
user_command (session * sess, char *cmd, char *buf)
{
	char tbuf[512];
	int parc;
	char *parv[MAX_TOKENS];

	split_cmd_parv(buf,&parc,parv);

	if (!auto_insert (tbuf, 2048, cmd, parc, parv, "",
			sess->channel, "", "", sess->server->nick, ""))
	{
		PrintText (sess, _("Bad arguments for user command.\n"));
		return;
	}

	handle_command (sess, tbuf, TRUE);
}

/* handle text entered without a CMDchar prefix */

static void
handle_say (session *sess, char *text, int check_spch)
{
	struct DCC *dcc;
	int parc=0;
	char *parv[MAX_TOKENS];
	char pdibuf_static[1024];
	char newcmd_static[1024];
	char tbuf_static[4096];
	char *pdibuf = pdibuf_static;
	char *newcmd = newcmd_static;
	char *tbuf = tbuf_static;
	size_t len;
	unsigned int newcmdlen = sizeof newcmd_static;

	if (strcmp (sess->channel, "(lastlog)") == 0)
	{
		lastlog (sess->lastlog_sess, text);
		return;
	}

	len = strlen (text);
	if (len >= sizeof pdibuf_static)
		pdibuf = malloc (len + 1);

	if (len + NICKLEN >= newcmdlen)
		newcmd = malloc (newcmdlen = (unsigned int)len + NICKLEN + 1);

	if (len * 2 >= sizeof tbuf_static)
		tbuf = malloc (len * 2 + 1);

	if (check_spch && prefs.perc_color)
		check_special_chars (text, prefs.perc_ascii);

	/* split the text into words and word_eol */
	process_data_init (pdibuf, text, parc, parv, TRUE);
	//irc_split(text, &parc, parv);

	/* a command of "" can be hooked for non-commands */
	if (plugin_emit_command (sess, "", text))
		goto xit;

	/* incase a plugin did /close */
	if (!is_session (sess))
		goto xit;

	if (!sess->channel[0] || sess->type == SESS_SERVER || sess->type == SESS_NOTICES || sess->type == SESS_SNOTICES)
	{
		notj_msg (sess);
		goto xit;
	}

	if (prefs.nickcompletion)
		perform_nick_completion (sess, text, newcmd);
	else
		safe_strcpy (newcmd, text, newcmdlen);

	text = newcmd;

	if (sess->type == SESS_DIALOG)
	{
		/* try it via dcc, if possible */
		dcc = dcc_write_chat (sess->channel, text);
		if (dcc)
		{
			inbound_chanmsg (sess->server, NULL, sess->channel,
								  sess->server->nick, text, TRUE, FALSE);
			set_topic (sess, net_ip (dcc->addr));
			goto xit;
		}
	}

	if (sess->server->connected)
	{
		unsigned int max;
		unsigned char t = 0;

		/* maximum allowed message text */
		/* :nickname!username@host.com PRIVMSG #channel :text\r\n */
		max = 512;
		max -= 16;	/* :, !, @, " PRIVMSG ", " ", :, \r, \n */
		max -= (unsigned int)strlen (sess->server->nick);
		max -= (unsigned int)strlen (sess->channel);
		if (sess->me && sess->me->hostname)
			max -= (unsigned int)strlen (sess->me->hostname);
		else
		{
			max -= 9;	/* username */
			max -= 65;	/* max possible hostname and '@' */
		}

		if (strlen (text) > max)
		{
			int i = 0, size;

			/* traverse the utf8 string and find the nearest 
			 * cut point that doesn't split 1 char in half 
			 * TODO: Try splitting on word boundaries*/
			while (1)
			{
				size = g_utf8_skip[((unsigned char *)text)[i]];
				if ((unsigned)(i + size) >= max)
					break;
				i += size;
			}
			max = i;
			t = text[max];
			text[max] = 0;			  /* insert a NULL 
							     terminator to 
							     shorten it */
		}

		inbound_chanmsg (sess->server, sess, sess->channel, sess->server->nick,
							  text, TRUE, FALSE);
		sess->server->p_message (sess->server, sess->channel, text);

		if (t)
		{
			text[max] = t;
			handle_say (sess, text + max, FALSE);
		}

	} else
	{
		notc_msg (sess);
	}

xit:
	if (pdibuf != pdibuf_static)
		free (pdibuf);

	if (newcmd != newcmd_static)
		free (newcmd);

	if (tbuf != tbuf_static)
		free (tbuf);
}

/* handle a command, without the '/' prefix */

int
handle_command (session *sess, char *cmd, int check_spch)
{
	struct popup *pop;
	int user_cmd = FALSE;
	static int command_level = 0;
	command *int_cmd;
	int ret = TRUE;
	char command[512];
	int i;

	if (command_level > 99)
	{
		fe_message (_("Too many recursive usercommands, aborting."), FALSE);
		return TRUE;
	}
	command_level++; /* below here MUST DEC command_level before return */


	/* find the command */
	for(i=0;cmd[i] && cmd[i]!=' ';i++)
		command[i]=cmd[i];
	command[i]='\0';
	
	if (check_spch && prefs.perc_color)
		check_special_chars (cmd, prefs.perc_ascii);

	if (plugin_emit_command (sess, command, cmd))
		goto xit;

	/* incase a plugin did /close */
	if (!is_session (sess))
		goto xit;

	/* first see if it's a userCommand */
	if ((pop = find_user_command(command)))
	{
		user_command (sess, pop->cmd, cmd);
		user_cmd = TRUE;
	}

	if (user_cmd)
		goto xit;

	/* now check internal commands */
	int_cmd = find_internal_command (command);

	if (int_cmd)
	{
		if (int_cmd->needserver && !sess->server->connected)
		{
			notc_msg (sess);
		} else if (int_cmd->needchannel && !sess->channel[0])
		{
			notj_msg (sess);
		} else
		{
			switch (int_cmd->callback (sess, command, cmd))
			{
			case FALSE:
				help (sess, int_cmd->name, TRUE);
				break;
			case 2:
				ret = FALSE;
				goto xit;
			}
		}
	} else
	{
		/* unknown command, just send it to the server and hope */
		if (!sess->server->connected)
			PrintText (sess, _("Unknown Command. Try /help\n"));
		else
			sess->server->p_raw (sess->server, cmd);
	}

xit:
	command_level--;

	return ret;
}

/* handle one line entered into the input box */

static int
handle_user_input (session *sess, char *text, int history, int nocommand)
{
	if (*text == '\0')
		return 1;

	if (history)
		history_add (&sess->history, text);

	/* is it NOT a command, just text? */
	if (nocommand || text[0] != prefs.cmdchar[0])
	{
		handle_say (sess, text, TRUE);
		return 1;
	}

	/* check for // */
	if (text[0] == prefs.cmdchar[0] && text[1] == prefs.cmdchar[0])
	{
		handle_say (sess, text + 1, TRUE);
		return 1;
	}

	if (prefs.cmdchar[0] == '/')
	{
		int i;
		const char *unix_dirs [] = {
			"/bin/", "/boot/", "/dev/",
			"/etc/", "/home/", "/lib/",
			"/lost+found/", "/mnt/", "/opt/",
			"/proc/", "/root/", "/sbin/",
			"/tmp/", "/usr/", "/var/",
			"/gnome/", NULL};
		for (i = 0; unix_dirs[i] != NULL; i++)
			if (strncmp (text, unix_dirs[i], strlen (unix_dirs[i]))==0)
			{
				handle_say (sess, text, TRUE);
				return 1;
			}
	}

	return handle_command (sess, text + 1, TRUE);
}

/* changed by Steve Green. Macs sometimes paste with imbedded \r */
void
handle_multiline (session *sess, char *cmd, int history, int nocommand)
{
	while (*cmd)
	{
		char *cr = cmd + strcspn (cmd, "\n\r");
		int end_of_string = *cr == 0;
		*cr = 0;
		if (!handle_user_input (sess, cmd, history, nocommand))
			return;
		if (end_of_string)
			break;
		cmd = cr + 1;
	}
}

