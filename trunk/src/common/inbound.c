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

/* black n white(0/1) are bad colors for nicks, and we'll use color 2 for us */
/* also light/dark gray (14/15) */
/* 5,7,8 are all shades of yellow which happen to look dman near the same */

static char rcolors[] = { 19, 20, 22, 24, 25, 26, 27, 28, 29 };

static int
color_of (char *name)
{
	int i = 0, sum = 0;

	while (name[i])
		sum += name[i++];
	sum %= sizeof (rcolors) / sizeof (char);
	return rcolors[sum];
}

void
clear_channel (rage_session *sess)
{
	if (sess->channel[0])
		strcpy (sess->waitchannel, sess->channel);
	sess->channel[0] = 0;
	sess->doing_who = FALSE;
	sess->done_away_check = FALSE;

	log_close (sess);

	if (sess->stack_timer)
	{
		g_source_remove(sess->stack_timer);
		sess->stack_timer = 0;
	}
	
	if (sess->stack_join)
	{
		g_queue_free(sess->stack_join);
		sess->stack_join = NULL;
	}

	if (sess->current_modes)
	{
		free (sess->current_modes);
		sess->current_modes = NULL;
	}

	if (sess->mode_timeout_tag)
	{
		g_source_remove (sess->mode_timeout_tag);
		sess->mode_timeout_tag = 0;
	}

	fe_clear_channel (sess);
	clear_user_list (sess);
	fe_set_nonchannel (sess, FALSE);
	fe_set_title (sess);
}

void
set_topic (rage_session *sess, char *topic)
{
	if (sess->topic)
		free (sess->topic);
	sess->topic = strdup (topic);
	fe_set_topic (sess, topic);
}

static rage_session *
find_session_from_nick (char *nick, server *serv)
{
	rage_session *sess;
	GSList *list = sess_list;

	sess = find_dialog (serv, nick);
	if (sess)
		return sess;

	if (serv->front_session)
	{
		if (find_name (serv->front_session, nick))
			return serv->front_session;
	}

	if (current_sess && current_sess->server == serv)
	{
		if (find_name (current_sess, nick))
			return current_sess;
	}

	while (list)
	{
		sess = list->data;
		if (sess->server == serv)
		{
			if (find_name (sess, nick))
				return sess;
		}
		list = list->next;
	}
	return 0;
}

static rage_session *
inbound_open_dialog (server *serv, char *from)
{
	rage_session *sess;

	sess = new_ircwindow (serv, from, SESS_DIALOG, 0);
	/* for playing sounds */
	EMIT_SIGNAL (XP_TE_OPENDIALOG, sess, NULL, NULL, NULL, NULL, 0);

	return sess;
}

void
inbound_privmsg (server *serv, char *from, char *ip, char *text, int id)
{
	rage_session *sess;
	char idtext[64];

	sess = find_dialog (serv, from);

	if (sess || prefs.autodialog)
	{
		/*0=ctcp  1=priv will set autodialog=0 here is flud detected */
		if (!sess)
		{
			if (flood_check (from, ip, serv, current_sess, 1))
				/* Create a dialog session */
				sess = inbound_open_dialog (serv, from);
			else
				sess = serv->server_session;
		}

		if (prefs.beepmsg || (sess && sess->beep))
			sound_beep (sess);

		if (ip && ip[0])
		{
			if (prefs.logging && sess->logfd != -1 &&
				(!sess->topic || strcmp(sess->topic, ip)))
			{
				char tbuf[1024];
				snprintf (tbuf, sizeof (tbuf), "[%s has address %s]\n", from, ip);
				write (sess->logfd, tbuf, (int)strlen (tbuf));
			}
			set_topic (sess, ip);
		}
		inbound_chanmsg (serv, NULL, NULL, from, text, FALSE, id);
		return;
	}

	if (id)
	{
		safe_strcpy (idtext, prefs.irc_id_ytext, sizeof (idtext));
	} else
	{
		safe_strcpy (idtext, prefs.irc_id_ntext, sizeof (idtext));
	}
	/* convert codes like %C,%U to the proper ones */
	check_special_chars (idtext, TRUE);

	sess = find_session_from_nick (from, serv);
	if (!sess)
	{
		sess = serv->front_session;

		if (prefs.beepmsg || (sess && sess->beep))
			sound_beep (sess);

		EMIT_SIGNAL (XP_TE_PRIVMSG, sess, from, text, idtext, NULL, 0);
		return;
	}

	if (prefs.beepmsg || sess->beep)
		sound_beep (sess);

	if (sess->type == SESS_DIALOG)
		EMIT_SIGNAL (XP_TE_DPRIVMSG, sess, from, text, idtext, NULL, 0);
	else
		EMIT_SIGNAL (XP_TE_PRIVMSG, sess, from, text, idtext, NULL, 0);
}

/* strncasecmp for utf8 strings, same syntax */
inline int
utf8_strncasecmp(const char *s1, const char *s2, size_t n)
{
	int retval = -1;
	while (((retval = g_unichar_tolower(g_utf8_get_char(s1)) - 
			g_unichar_tolower(g_utf8_get_char(s2))) == 0) && (--n > 0))
	{
			s1 = g_utf8_find_next_char(s1, NULL);
			s2 = g_utf8_find_next_char(s2, NULL);
	}
	return retval;
}

/* case_strchr for utf8 strings, works like strchr */
inline char *
utf8_case_strchr(char *buf, const char *s)
{
	gunichar c = g_unichar_tolower(g_utf8_get_char(s));
	while (buf && *buf && g_unichar_tolower(g_utf8_get_char(buf)) != c)
		buf = g_utf8_find_next_char(buf, NULL);
	return buf;
}

static int
SearchNick (char *text, char *nicks)
{
	char *nstart, *nend, *tmp;

	nstart = nicks;
	while (nstart)
	{
		nend = strchr (nstart, ',');
		tmp = utf8_case_strchr(text, nstart);
		while (tmp && *tmp) /* seems like glib never returns NULL */
		{
			/* We need a color stripped strncasecmp and it will work just fine */
			if (utf8_strncasecmp(tmp, nstart, nend ? nend - nstart : strlen(nstart)) == 0)
				return 1;
			tmp = g_utf8_find_next_char(tmp, NULL);
			tmp = tmp ? utf8_case_strchr(tmp, nstart) : NULL;
		}
		nstart = nend ? g_utf8_find_next_char(nend, NULL) : NULL;
	}
	return 0;
}

static int
is_hilight (char *text, rage_session *sess, server *serv)
{
	if ((SearchNick (text, serv->nick)) || SearchNick (text, prefs.bluestring))
	{
#ifdef WIN32
		if (sess != current_tab)
			sess->nick_said = TRUE;
		fe_set_hilight (sess);
#else
		if (sess != current_tab)
		{
			sess->nick_said = TRUE;
			fe_set_hilight (sess);
		}
#endif
		return 1;
	}
	return 0;
}

void
inbound_action (rage_session *sess, char *chan, char *from, char *text, int fromme)
{
	rage_session *def = sess;
	server *serv = sess->server;
	int beep = FALSE;
	int hilight = FALSE;

	if (!fromme)
	{
		if (is_channel (serv, chan))
		{
			sess = find_channel (serv, chan);
			beep = prefs.beepchans;
		} else
		{
			/* it's a private action! */
			beep = prefs.beepmsg;
			/* find a dialog tab for it */
			sess = find_dialog (serv, from);
			/* if non found, open a new one */
			if (!sess && prefs.autodialog)
				sess = inbound_open_dialog (serv, from);
		}
	}

	if (!sess)
		sess = def;

	if (sess != current_tab)
	{
		if (fromme)
		{
			sess->msg_said = FALSE;
			sess->new_data = TRUE;
		} else
		{
			sess->msg_said = TRUE;
			sess->new_data = FALSE;
		}
	}

	if (!fromme)
	{
		hilight = is_hilight (text, sess, serv);
		if (hilight && prefs.beephilight)
			beep = TRUE;

		if (beep || sess->beep)
			sound_beep (sess);

		if (hilight)
		{
			EMIT_SIGNAL (XP_TE_HCHANACTION, sess, from, text, NULL, NULL, 0);
			return;
		}
	}

	if (prefs.colorednicks)
	{
		char tbuf[NICKLEN + 4];
		snprintf (tbuf, sizeof (tbuf), "\003%d%s", color_of (from), from);
		EMIT_SIGNAL (XP_TE_CHANACTION, sess, tbuf, text, NULL, NULL, 0);
	} else
	{
		EMIT_SIGNAL (XP_TE_CHANACTION, sess, from, text, NULL, NULL, 0);
	}
}

void
inbound_chanmsg (server *serv, rage_session *sess, char *chan, char *from, char *text, char fromme, int id)
{
	struct User *user;
	int hilight = FALSE;
	char nickchar[2] = "\000";
	char idtext[64];

	if (!sess)
	{
		if (chan)
		{
			sess = find_channel (serv, chan);
			if (!sess && !is_channel (serv, chan))
				sess = find_dialog (serv, chan);
		} else
			sess = find_dialog (serv, from);
		if (!sess)
			return;
	}

	if (sess != current_tab)
	{
		sess->msg_said = TRUE;
		sess->new_data = FALSE;
	}

	user = find_name (sess, from);
	if (user)
	{
		nickchar[0] = user->prefix[0];
		user->lasttalk = time (0);
	}

	if (fromme)
	{
  		if (prefs.auto_unmark_away && serv->is_away)
			sess->server->p_set_back (sess->server);
		EMIT_SIGNAL (XP_TE_UCHANMSG, sess, from, text, nickchar, NULL, 0);
		return;
	}

	if (id)
	{
		safe_strcpy (idtext, prefs.irc_id_ytext, sizeof (idtext));
	}
	else
	{
		safe_strcpy (idtext, prefs.irc_id_ntext, sizeof (idtext));
	}
	/* convert codes like %C,%U to the proper ones */
	check_special_chars (idtext, TRUE);

	if (sess->type != SESS_DIALOG)
		if (prefs.beepchans || sess->beep)
			sound_beep (sess);

	if (is_hilight (text, sess, serv))
	{
		hilight = TRUE;
		if (prefs.beephilight)
			sound_beep (sess);
	}
	if (sess->type == SESS_DIALOG)
		EMIT_SIGNAL (XP_TE_DPRIVMSG, sess, from, text, idtext, NULL, 0);
	else if (hilight)
		EMIT_SIGNAL (XP_TE_HCHANMSG, sess, from, text, nickchar, idtext, 0);
	else if (prefs.colorednicks)
	{
		char tbuf[NICKLEN + 4];
		snprintf (tbuf, sizeof (tbuf), "\003%d%s", color_of (from), from);
		EMIT_SIGNAL (XP_TE_CHANMSG, sess, tbuf, text, nickchar, idtext, 0);
	}
	else
		EMIT_SIGNAL (XP_TE_CHANMSG, sess, from, text, nickchar, idtext, 0);
}

void
inbound_newnick (server *serv, char *nick, char *newnick, int quiet)
{
	int me = FALSE;
	rage_session *sess;
	GSList *list = sess_list;

	if (!serv->p_cmp (nick, serv->nick))
	{
		me = TRUE;
		safe_strcpy (serv->nick, newnick, NICKLEN);
	}

	while (list)
	{
		sess = list->data;
		if (sess->server == serv)
		{
			if (change_nick (sess, nick, newnick) || (me && sess->type == SESS_SERVER))
			{
				if (!quiet)
				{
					if (me)
						EMIT_SIGNAL (XP_TE_UCHANGENICK, sess, nick, newnick, NULL,
										 NULL, 0);
					else
						EMIT_SIGNAL (XP_TE_CHANGENICK, sess, nick, newnick, NULL,
										 NULL, 0);
				}
			}
			if (sess->type == SESS_DIALOG && !serv->p_cmp (sess->channel, nick))
			{
				safe_strcpy (sess->channel, newnick, CHANLEN);
				fe_set_channel (sess);
			}
			fe_set_title (sess);
		}
		list = list->next;
	}

	dcc_change_nick (serv, nick, newnick);

	if (me)
		fe_set_nick (serv, newnick);
}

/* find a "<none>" tab */
static rage_session *
find_unused_session (server *serv)
{
	rage_session *sess;
	GSList *list = sess_list;
	while (list)
	{
		sess = (rage_session *) list->data;
		if (sess->type == SESS_CHANNEL && sess->channel[0] == 0 &&
			 sess->server == serv)
		{
			if (sess->waitchannel[0] == 0)
				return sess;
		}
		list = list->next;
	}
	return 0;
}

static rage_session *
find_session_from_waitchannel (char *chan, struct server *serv)
{
	rage_session *sess;
	GSList *list = sess_list;
	while (list)
	{
		sess = (rage_session *) list->data;
		if (sess->server == serv && sess->channel[0] == 0 && sess->type == SESS_CHANNEL)
		{
			if (!serv->p_cmp (chan, sess->waitchannel))
				return sess;
		}
		list = list->next;
	}
	return 0;
}

void
inbound_ujoin (server *serv, char *chan, char *nick, char *ip)
{
	rage_session *sess;

	/* already joined? probably a bnc */
	sess = find_channel (serv, chan);
	if (!sess)
	{
		/* see if a window is waiting to join this channel */
		sess = find_session_from_waitchannel (chan, serv);
		if (!sess)
		{
			/* find a "<none>" tab and use that */
			sess = find_unused_session (serv);
			if (!sess)
				/* last resort, open a new tab/window */
				sess = new_ircwindow (serv, chan, SESS_CHANNEL, 1);
		}
	}

	safe_strcpy (sess->channel, chan, CHANLEN);

	fe_set_channel (sess);
	fe_set_title (sess);
	fe_set_nonchannel (sess, TRUE);
	clear_user_list (sess);

	if (prefs.logging)
		log_open (sess);

	sess->waitchannel[0] = 0;
	sess->ignore_mode = TRUE;
	sess->ignore_names = TRUE;
	sess->end_of_names = FALSE;

	/* sends a MODE */
	serv->p_join_info (sess->server, chan);

	EMIT_SIGNAL (XP_TE_UJOIN, sess, nick, chan, ip, NULL, 0);

	if (prefs.userhost)
	{
		/* sends WHO #channel */
		serv->p_user_list (sess->server, chan);
		sess->doing_who = TRUE;
	}
}

void
inbound_ukick (server *serv, char *chan, char *kicker, char *reason)
{
	rage_session *sess = find_channel (serv, chan);
	if (sess)
	{
		EMIT_SIGNAL (XP_TE_UKICK, sess, serv->nick, chan, kicker, reason, 0);
		clear_channel (sess);
		if (prefs.autorejoin)
		{
			serv->p_join (serv, chan, sess->channelkey);
			safe_strcpy (sess->waitchannel, chan, CHANLEN);
		}
	}
}

void
inbound_upart (server *serv, char *chan, char *ip, char *reason)
{
	rage_session *sess = find_channel (serv, chan);
	if (sess)
	{
		if (*reason)
			EMIT_SIGNAL (XP_TE_UPARTREASON, sess, serv->nick, ip, chan, reason,
							 0);
		else
			EMIT_SIGNAL (XP_TE_UPART, sess, serv->nick, ip, chan, NULL, 0);
		clear_channel (sess);
	}
}

void
inbound_nameslist (server *serv, char *chan, char *names)
{
	rage_session *sess;
	char name[NICKLEN];
	int pos = 0;

	sess = find_channel (serv, chan);
	if (!sess)
	{
		EMIT_SIGNAL (XP_TE_USERSONCHAN, serv->server_session, chan, names, NULL,
						 NULL, 0);
		return;
	}
	if (!sess->ignore_names)
		EMIT_SIGNAL (XP_TE_USERSONCHAN, sess, chan, names, NULL, NULL, 0);

	if (sess->end_of_names)
	{
		sess->end_of_names = FALSE;
		clear_user_list (sess);
	}

	while (1)
	{
		switch (*names)
		{
		case 0:
			name[pos] = 0;
			if (pos != 0)
				add_name (sess, name, 0);
			return;
		case ' ':
			name[pos] = 0;
			pos = 0;
			add_name (sess, name, 0);
			break;
		default:
			name[pos] = *names;
			if (pos < (NICKLEN-1))
				pos++;
		}
		names++;
	}
}

void
inbound_topic (server *serv, char *chan, char *topic_text)
{
	rage_session *sess = find_channel (serv, chan);
	char *new_topic;

	if (sess)
	{
		new_topic = strip_color (topic_text);
		set_topic (sess, new_topic);
		free (new_topic);
	} else
		sess = serv->server_session;

	EMIT_SIGNAL (XP_TE_TOPIC, sess, chan, topic_text, NULL, NULL, 0);
}

void
inbound_topicnew (server *serv, char *nick, char *chan, char *topic)
{
	rage_session *sess;
	char *new_topic;

	sess = find_channel (serv, chan);
	if (sess)
	{
		new_topic = strip_color (topic);
		set_topic (sess, new_topic);
		free (new_topic);
		EMIT_SIGNAL (XP_TE_NEWTOPIC, sess, nick, topic, chan, NULL, 0);
	}
}

int
handle_mjoin(rage_session *sess)
{
	char buf[2048];
	struct User *tmp = NULL;
	int len;

	if (g_queue_get_length(sess->stack_join) > 1)
	{
		while(!g_queue_is_empty(sess->stack_join))
		{
			len = 0;

			while ((tmp = g_queue_pop_head(sess->stack_join)) &&
					len <= (sizeof(buf) - (NICKLEN +3)))
			{
				len += sprintf (buf + len, "%s", tmp->nick);
				if (!g_queue_is_empty(sess->stack_join))
					len += sprintf (buf + len, ", ");
			}
			EMIT_SIGNAL (sess->server->split_timer ? XP_TE_NS_JOIN : XP_TE_MJOIN, 
					sess, sess->channel, buf, NULL, NULL, 0);
		}
	}
	else
	{
		tmp = g_queue_pop_head(sess->stack_join);
		/* We might not have added anything due to netsplits */
		if (tmp)
			EMIT_SIGNAL (XP_TE_JOIN, sess, tmp->nick, 
					sess->channel, tmp->hostname, NULL, 0);
	}
	g_queue_free(sess->stack_join);
	sess->stack_join = NULL;
	sess->stack_timer = 0; /* Will be removed when we return FALSE */
	return FALSE;
}

/* FIXME: There has to be a better way but currently i'm at a loss to what
 * it would be... */
void
check_mjoin (rage_session *sess)
{
	if (sess->stack_timer)
	{
		g_source_remove(sess->stack_timer);
		handle_mjoin(sess);
	}
}

void
inbound_join (server *serv, char *chan, char *user, char *ip)
{
	rage_session *sess = find_channel (serv, chan);
	if (sess)
	{
		if (!sess->hide_join_part)
		{
			struct User *user_node;
			GList *tmp;
			
			if (sess->stack_timer)
				g_source_remove(sess->stack_timer);
			else
				sess->stack_join = g_queue_new();
			
			user_node = add_name (sess, user, ip);
			
			if (sess->server->split_queue && 
					(tmp = g_queue_find_custom(sess->server->split_queue, user, 
								   (GCompareFunc)serv->p_cmp)))
			{
				g_free(tmp->data);
				g_queue_delete_link(sess->server->split_queue, tmp);
			}
			else
				g_queue_push_head(sess->stack_join, user_node);
			
			sess->stack_timer = g_timeout_add(250, 
					(GSourceFunc)handle_mjoin, sess);
		}
		else
			add_name (sess, user, ip);
	}
}

void
inbound_kick (server *serv, char *chan, char *user, char *kicker, char *reason)
{
	rage_session *sess = find_channel (serv, chan);
	if (sess)
	{
		check_mjoin(sess);
		EMIT_SIGNAL (XP_TE_KICK, sess, kicker, user, chan, reason, 0);
		sub_name (sess, user);
	}
}

void
inbound_part (server *serv, char *chan, char *user, char *ip, char *reason)
{
	rage_session *sess = find_channel (serv, chan);
	if (sess)
	{
		check_mjoin(sess);
		if (!sess->hide_join_part)
		{
			if (*reason)
				EMIT_SIGNAL (XP_TE_PARTREASON, sess, user, ip, chan, reason, 0);
			else
				EMIT_SIGNAL (XP_TE_PART, sess, user, ip, chan, NULL, 0);
		}
		sub_name (sess, user);
	}
}

void
inbound_topictime (server *serv, char *chan, char *nick, time_t stamp)
{
	char *tim = ctime (&stamp);
	rage_session *sess = find_channel (serv, chan);

	if (!sess)
		sess = serv->server_session;

	tim[24] = 0;	/* get rid of the \n */
	EMIT_SIGNAL (XP_TE_TOPICDATE, sess, chan, nick, tim, NULL, 0);
}

void
set_server_name (struct server *serv, char *name)
{
	GSList *list = sess_list;
	rage_session *sess;

	if (name[0] == 0)
		name = serv->hostname;

	/* strncpy parameters must NOT overlap */
	if (name != serv->servername)
	{
		safe_strcpy (serv->servername, name, sizeof (serv->servername));
	}

	while (list)
	{
		sess = (rage_session *) list->data;
		if (sess->server == serv)
			fe_set_title (sess);
		list = list->next;
	}

	if (serv->server_session->type == SESS_SERVER)
	{
		if (serv->network)
		{
			safe_strcpy (serv->server_session->channel, ((ircnet *)serv->network)->name, CHANLEN);
		} else
		{
			safe_strcpy (serv->server_session->channel, name, CHANLEN);
		}
		fe_set_channel (serv->server_session);
	}
}

int
handle_netsplit(server *serv)
{
	char buf[2048], *tmp;
	int len;

	if (g_queue_is_empty(serv->split_queue))
		EMIT_SIGNAL (XP_TE_NS_OVER, serv->front_session, NULL, NULL, NULL, NULL, 0);
	else
	{
		while(!g_queue_is_empty(serv->split_queue))
		{
			len = 0;
		
			while ((tmp = g_queue_pop_head(serv->split_queue)) && 
					len <= (sizeof(buf) - (NICKLEN +3)))
			{
				len += sprintf (buf + len, "%s", tmp);
				if (!g_queue_is_empty(serv->split_queue))
					len += sprintf (buf + len, ", ");
				g_free(tmp);
			}
			EMIT_SIGNAL (XP_TE_NS_GONE, serv->front_session, buf, NULL, NULL, NULL, 0);
		}
	}
	g_queue_free(serv->split_queue);
	serv->split_queue = NULL;
	g_free(serv->split_reason);
	serv->split_reason = NULL;
	serv->split_timer = 0; /* Will be removed when we return FALSE */
	return FALSE;
}

void
inbound_quit (server *serv, char *nick, char *ip, char *reason)
{
	GSList *list = sess_list;
	rage_session *sess = NULL;
	int was_on_front_session = FALSE;
	int netsplit = FALSE;
	char *seperator, *p;
	
	if (serv->split_reason && (strcmp(serv->split_reason, reason) == 0))
		netsplit = TRUE;
	else if ((seperator = strchr(reason, ' ')))
	{
		int tld = 0, space = FALSE;
		*seperator = 0;

		if((p = strchr(reason, '.')))
		{
			for (; *p; p++)
			{
				if (*p == '.')
					tld = 0;
				else
					tld++;
			}
			if ((tld > 1) && (tld < 5))
			{
				if ((p = strchr(seperator +1, '.')))
				{
					for (; *p; p++)
					{
						if (*p == ' ')
						{
							space = TRUE;
							break;
						}
						if (*p == '.')
							tld = 0;
						else
							tld++;
					}
					if (!space && (tld > 1) && (tld < 6))
						netsplit = TRUE;
				}
			}
		}
		
		if (netsplit)
		{	
			if (serv->split_queue)
			{
				if (serv->split_timer)
					g_source_remove(serv->split_timer);
				handle_netsplit(serv);
			}

			serv->split_queue = g_queue_new();

			EMIT_SIGNAL (XP_TE_NS_START, serv->front_session, reason, 
					seperator+1, NULL, NULL, 0);
		
			*seperator = ' ';
			serv->split_reason = g_strdup(reason);
		}
		else
			*seperator = ' ';
	}
	
	if (netsplit)
	{
		g_queue_push_head(serv->split_queue, g_strdup(nick));
		if (serv->split_timer)
			g_source_remove(serv->split_timer);
		serv->split_timer = g_timeout_add(prefs.netsplit_time, 
				(GSourceFunc)handle_netsplit, serv);
	}

	for (; list; list = list->next)
	{
		sess = (rage_session *) list->data;
		if (sess->server == serv)
		{
			check_mjoin(sess);
 			if (sess == current_sess)
 				was_on_front_session = TRUE;
			if (sub_name (sess, nick))
			{
				if (!netsplit && !sess->hide_join_part)
					EMIT_SIGNAL (XP_TE_QUIT, sess, nick, reason, ip, NULL, 0);
			}
			else if (sess->type == SESS_DIALOG && 
					!serv->p_cmp (sess->channel, nick))
				EMIT_SIGNAL (XP_TE_QUIT, sess, nick, reason, ip, NULL, 0);
		}
	}

	notify_set_offline (serv, nick, was_on_front_session);
}

void
inbound_ping_reply (rage_session *sess, char *timestring, char *from)
{
	unsigned long tim, nowtim, dif;
	int lag = 0;
	char outbuf[64];

	if (strncmp (timestring, "LAG", 3) == 0)
	{
		timestring += 3;
		lag = 1;
	}

	tim = strtoul (timestring, NULL, 10);
	nowtim = make_ping_time ();
	dif = nowtim - tim;

	sess->server->ping_recv = time (0);

	if (lag)
	{
		sess->server->lag_sent = 0;
		snprintf (outbuf, sizeof (outbuf), "%ld.%ld", dif / 100000, (dif / 10000) % 100);
		fe_set_lag (sess->server, (int)((float)atof (outbuf)));
		return;
	}

	if (atol (timestring) == 0)
	{
		if (sess->server->lag_sent)
			sess->server->lag_sent = 0;
		else
			EMIT_SIGNAL (XP_TE_PINGREP, sess, from, "?", NULL, NULL, 0);
	} else
	{
		snprintf (outbuf, sizeof (outbuf), "%ld.%ld%ld", dif / 1000000, (dif / 100000) % 10, dif % 10);
		EMIT_SIGNAL (XP_TE_PINGREP, sess, from, outbuf, NULL, NULL, 0);
	}
}

static rage_session *
find_session_from_type (int type, server *serv)
{
	rage_session *sess;
	GSList *list = sess_list;
	while (list)
	{
		sess = list->data;
		if (sess->type == type && serv == sess->server)
			return sess;
		list = list->next;
	}
	return 0;
}

void
inbound_notice (server *serv, char *to, char *nick, char *msg, char *ip, int server_notice, int id)
{
	char *po,*ptr=to;
	rage_session *sess = 0;
	int new_session = 0;

	if (is_channel (serv, ptr))
		sess = find_channel (serv, ptr);

	if (!sess && ptr[0] == '@')
	{
		ptr++;
		sess = find_channel (serv, ptr);
	}

	if (!sess && ptr[0] == '+')
	{
		ptr++;
		sess = find_channel (serv, ptr);
	}

	if (!sess)
	{
		register unsigned int oldh = 0;
		ptr = 0;
		if (prefs.snotices_tab && server_notice)
		{
			sess = find_session_from_type (SESS_SNOTICES, serv);
			if (!sess)
			{
				oldh = prefs.hideuserlist;
				prefs.hideuserlist = 1;
				sess = new_ircwindow (serv, "(snotices)", SESS_SNOTICES, 0);
				new_session = 1;
			}
			/* Avoid redundancy with some Undernet notices */
			if (!strncmp (msg, "*** Notice -- ", 14))
				msg += 14;
		}
		else if (prefs.notices_tab && !server_notice)
		{
			sess = find_session_from_type (SESS_NOTICES, serv);
			if (!sess)
			{
				oldh = prefs.hideuserlist;
				prefs.hideuserlist = 1;
				sess = new_ircwindow (serv, "(notices)", SESS_NOTICES, 0);
				new_session = 1;
			}
		}
		if (new_session)
		{
			prefs.hideuserlist = oldh;
			fe_set_channel (sess);
			fe_set_title (sess);
			fe_set_nonchannel (sess, FALSE);
			clear_user_list (sess);
			if (prefs.logging)
				log_open (sess);
		}
		(void)oldh;
		
		if (!sess)
		{
			if (server_notice)	
				sess = serv->server_session;
			else
			{
				sess = find_session_from_nick (nick, serv);
				if (!sess)
					sess = serv->front_session;
			}
		}
	}

	if (msg[0] == 1)
	{
		msg++;
		if (!strncmp (msg, "PING", 4))
		{
			inbound_ping_reply (sess, msg + 5, nick);
			return;
		}
	}
	po = strchr (msg, '\001');
	if (po)
		po[0] = 0;

	if (server_notice)
		EMIT_SIGNAL (XP_TE_SERVNOTICE, sess, msg, nick, NULL, NULL, 0);
	else if (ptr)
		EMIT_SIGNAL (XP_TE_CHANNOTICE, sess, nick, to, msg, NULL, 0);
	else
		EMIT_SIGNAL (XP_TE_NOTICE, sess, nick, msg, NULL, NULL, 0);
}

void
inbound_away (server *serv, char *nick, char *msg)
{
	struct away_msg *away = find_away_message (serv, nick);
	rage_session *sess = NULL;
	GSList *list;

	if (away && !strcmp (msg, away->message))	/* Seen the msg before? */
	{
		if (prefs.show_away_once && !serv->inside_whois)
			return;
	} else
	{
		save_away_message (serv, nick, msg);
	}

	if (!serv->inside_whois)
		sess = find_session_from_nick (nick, serv);
	if (!sess)
		sess = serv->server_session;

	EMIT_SIGNAL (XP_TE_WHOIS5, sess, nick, msg, NULL, NULL, 0);

	list = sess_list;
	while (list)
	{
		sess = list->data;
		if (sess->server == serv)
			userlist_set_away (sess, nick, TRUE);
		list = list->next;
	}
}

int
inbound_nameslist_end (server *serv, char *chan)
{
	rage_session *sess;
	GSList *list;

	if (!strcmp (chan, "*"))
	{
		list = sess_list;
		while (list)
		{
			sess = list->data;
			if (sess->server == serv)
			{
				sess->end_of_names = TRUE;
				sess->ignore_names = FALSE;
			}
			list = list->next;
		}
		return TRUE;
	}
	sess = find_channel (serv, chan);
	if (sess)
	{
		sess->end_of_names = TRUE;
		sess->ignore_names = FALSE;
		return TRUE;
	}
	return FALSE;
}

static void
check_willjoin_channels (server *serv)
{
	char *po;
	rage_session *sess;
	GSList *list = sess_list;

	while (list)
	{
		sess = list->data;
		if (sess->server == serv)
		{
			if (sess->willjoinchannel[0] != 0)
			{
				strcpy (sess->waitchannel, sess->willjoinchannel);
				sess->willjoinchannel[0] = 0;
				serv->p_join (serv, sess->waitchannel, sess->channelkey);
				po = strchr (sess->waitchannel, ',');
				if (po)
					*po = 0;
				po = strchr (sess->waitchannel, ' ');
				if (po)
					*po = 0;
			}
		}
		list = list->next;
	}
}

void
inbound_next_nick (rage_session *sess, char *nick)
{
	sess->server->nickcount++;

	switch (sess->server->nickcount)
	{
	case 2:
		sess->server->p_change_nick (sess->server, prefs.nick2);
		EMIT_SIGNAL (XP_TE_NICKCLASH, sess, nick, prefs.nick2, NULL, NULL, 0);
		break;

	case 3:
		sess->server->p_change_nick (sess->server, prefs.nick3);
		EMIT_SIGNAL (XP_TE_NICKCLASH, sess, nick, prefs.nick3, NULL, NULL, 0);
		break;

	default:
		EMIT_SIGNAL (XP_TE_NICKFAIL, sess, NULL, NULL, NULL, NULL, 0);
	}
}

void
do_dns (rage_session *sess, char *nick, char *host)
{
	char *po;
	char tbuf[1024];

	po = strrchr (host, '@');
	if (po)
		host = po + 1;
	EMIT_SIGNAL (XP_TE_RESOLVINGUSER, sess, nick, host, NULL, NULL, 0);
	snprintf (tbuf, sizeof (tbuf), "exec -d %s %s", prefs.dnsprogram, host);
	handle_command (sess, tbuf, FALSE);
}

static void
set_default_modes (server *serv)
{
	char modes[8];

	modes[0] = '+';
	modes[1] = '\0';

	if (prefs.wallops)
		strcat (modes, "w");
	if (prefs.servernotice)
		strcat (modes, "s");
	if (prefs.invisible)
		strcat (modes, "i");

	if (modes[1] != '\0')
	{
		serv->p_mode (serv, serv->nick, modes);
	}
}

void
inbound_login_start (rage_session *sess, char *nick, char *servname)
{
	inbound_newnick (sess->server, sess->server->nick, nick, TRUE);
	set_server_name (sess->server, servname);
	if (sess->type == SESS_SERVER && prefs.logging)
		log_open (sess);
	/* reset our away status */
	if (sess->server->reconnect_away)
	{
		handle_command (sess->server->server_session, "away", FALSE);
		sess->server->reconnect_away = FALSE;
	}
}

static void
inbound_set_all_away_status (server *serv, char *nick, unsigned int status)
{
	GSList *list;
	rage_session *sess;

	list = sess_list;
	while (list)
	{
		sess = list->data;
		if (sess->server == serv)
			userlist_set_away (sess, nick, status);
		list = list->next;
	}
}

void
inbound_uaway (server *serv)
{
	serv->is_away = TRUE;
	serv->away_time = time (NULL);
	fe_set_away (serv);

	inbound_set_all_away_status (serv, serv->nick, 1);
}

void
inbound_uback (server *serv)
{
	serv->is_away = FALSE;
	serv->reconnect_away = FALSE;
	fe_set_away (serv);

	inbound_set_all_away_status (serv, serv->nick, 0);
}

void
inbound_foundip (rage_session *sess, char *ip)
{
	struct hostent *HostAddr;

	HostAddr = gethostbyname (ip);
	if (HostAddr)
	{
		prefs.dcc_ip = ((struct in_addr *) HostAddr->h_addr)->s_addr;
		EMIT_SIGNAL (XP_TE_FOUNDIP, sess,
						 inet_ntoa (*((struct in_addr *) HostAddr->h_addr)),
						 NULL, NULL, NULL, 0);
	}
}

void
inbound_user_info_start (rage_session *sess, char *nick)
{
	/* set away to FALSE now, 301 may turn it back on */
	inbound_set_all_away_status (sess->server, nick, 0);
}

int
inbound_user_info (rage_session *sess, char *chan, char *user, char *host,
						 char *servname, char *nick, char *realname,
						 unsigned int away)
{
	server *serv = sess->server;
	rage_session *who_sess;
	char *uhost;

	who_sess = find_channel (serv, chan);
	if (who_sess)
	{
		if (user && host)
		{
			uhost = malloc (strlen (user) + strlen (host) + 2);
			sprintf (uhost, "%s@%s", user, host);
			if (!userlist_add_hostname (who_sess, nick, uhost, realname, servname, away))
			{
				if (!who_sess->doing_who)
				{
					free (uhost);
					return 0;
				}
			}
			free (uhost);
		} else
		{
			if (!userlist_add_hostname (who_sess, nick, NULL, realname, servname, away))
			{
				if (!who_sess->doing_who)
					return 0;
			}
		}
	} else
	{
		if (!serv->doing_dns)
			return 0;
		if (nick && host)
			do_dns (sess, nick, host);
	}
	return 1;
}

void
inbound_banlist (rage_session *sess, time_t stamp, char *chan, char *mask, char *banner)
{
	char *time_str = ctime (&stamp);
	server *serv = sess->server;

	time_str[19] = 0;	/* get rid of the \n */
	if (stamp == 0)
		time_str = "";

	sess = find_channel (serv, chan);
	if (!sess)
		sess = serv->front_session;
   if (!fe_is_banwindow (sess))
		EMIT_SIGNAL (XP_TE_BANLIST, sess, chan, mask, banner, time_str, 0);
	else
		fe_add_ban_list (sess, mask, banner, time_str);
}

/* execute 1 end-of-motd command */

static int
inbound_exec_eom_cmd (char *str, void *sess)
{
	handle_command (sess, (str[0] == '/') ? str + 1 : str, TRUE);
	return 1;
}

void
inbound_login_end (rage_session *sess, char *text)
{
	server *serv = sess->server;

	if (!serv->end_of_motd)
	{
		if (prefs.ip_from_server && serv->use_who)
		{
			serv->skip_next_userhost = TRUE;
			serv->p_get_ip_uh (serv, serv->nick);	/* sends USERHOST mynick */
		}
		set_default_modes (serv);

		if (serv->network)
		{
			/* there may be more than 1, separated by \n */
			if (((ircnet *)serv->network)->command)
				token_foreach (((ircnet *)serv->network)->command, '\n',
									inbound_exec_eom_cmd, sess);

			/* send nickserv password */
			if (((ircnet *)serv->network)->nickserv)
				serv->p_nickserv (serv, ((ircnet *)serv->network)->nickserv);
		}

		check_willjoin_channels (serv);
		if (isupport(serv, "WATCH"))
			notify_send_watches (serv);
		serv->end_of_motd = TRUE;
	}
	if (prefs.skipmotd && !serv->motd_skipped)
	{
		serv->motd_skipped = TRUE;
		EMIT_SIGNAL (XP_TE_MOTDSKIP, serv->server_session, NULL, NULL,
						 NULL, NULL, 0);
		return;
	}
	EMIT_SIGNAL (XP_TE_MOTD, serv->server_session, text, NULL,
					 NULL, NULL, 0);
}
