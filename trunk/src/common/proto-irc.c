/* X-Chat
 * Copyright (C) 2002 Peter Zelezny.
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

/* IRC RFC1459(+commonly used extensions) protocol implementation */

#include "rage.h"

static void
irc_login (server *serv, char *user, char *realname)
{
	char hostname[128];

	if (serv->password[0])
		tcp_sendf (serv, "PASS %s\r\n", serv->password);

	gethostname (hostname, sizeof (hostname) - 1);
	hostname[sizeof (hostname) - 1] = 0;
	if (hostname[0] == 0)
		strcpy (hostname, "0");

	tcp_sendf (serv,
				  "NICK %s\r\n"
				  "USER %s %s %s :%s\r\n",
				  serv->nick, user, hostname, serv->servername, realname);
}

static void
irc_nickserv (server *serv, char *pass)
{
	/* FIXME: this is a merge, but the code is fscking stupid! */
	switch (serv->nickservtype)
	{
		case 0:
			tcp_sendf (serv, "PRIVMSG NickServ :identify %s\r\n", pass);
			break;
		case 1:
			tcp_sendf (serv, "NICKSERV :identify %s\r\n", pass);
			break;
		case 2:
			tcp_sendf (serv, "NS :identify %s\r\n", pass);
	}
}

static void
irc_join (server *serv, char *channel, char *key)
{
	if (key[0])
		tcp_sendf (serv, "JOIN %s %s\r\n", channel, key);
	else
		tcp_sendf (serv, "JOIN %s\r\n", channel);
}

static void
irc_part (server *serv, char *channel, char *reason)
{
	if (reason[0])
		tcp_sendf (serv, "PART %s :%s\r\n", channel, reason);
	else
		tcp_sendf (serv, "PART %s\r\n", channel);
}

static void
irc_quit (server *serv, char *reason)
{
	if (reason[0])
		tcp_sendf (serv, "QUIT :%s\r\n", reason);
	else
		tcp_send_len (serv, "QUIT\r\n", 6);
}

static void
irc_set_back (server *serv)
{
	tcp_send_len (serv, "AWAY\r\n", 6);
}

static void
irc_set_away (server *serv, char *reason)
{
	tcp_sendf (serv, "AWAY :%s\r\n", reason);
}

static void
irc_ctcp (server *serv, char *to, char *msg)
{
	tcp_sendf (serv, "PRIVMSG %s :\001%s\001\r\n", to, msg);
}

static void
irc_nctcp (server *serv, char *to, char *msg)
{
	tcp_sendf (serv, "NOTICE %s :\001%s\001\r\n", to, msg);
}

static void
irc_cycle (server *serv, char *channel, char *key)
{
	tcp_sendf (serv, "PART %s\r\nJOIN %s %s\r\n", channel, channel, key);
}

static void
irc_kick (server *serv, char *channel, char *nick, char *reason)
{
	if (reason[0])
		tcp_sendf (serv, "KICK %s %s :%s\r\n", channel, nick, reason);
	else
		tcp_sendf (serv, "KICK %s %s\r\n", channel, nick);
}

static void
irc_invite (server *serv, char *channel, char *nick)
{
	tcp_sendf (serv, "INVITE %s %s\r\n", nick, channel);
}

static void
irc_mode (server *serv, char *target, char *mode)
{
	tcp_sendf (serv, "MODE %s %s\r\n", target, mode);
}

/* find channel info when joined */

static void
irc_join_info (server *serv, char *channel)
{
	tcp_sendf (serv, "MODE %s\r\n", channel);
}

/* initiate userlist retreival */

static void
irc_user_list (server *serv, char *channel)
{
	tcp_sendf (serv, "WHO %s\r\n", channel);
}

/* userhost */

static void
irc_userhost (server *serv, char *nick)
{
	tcp_sendf (serv, "USERHOST %s\r\n", nick);
}

static void
irc_away_status (server *serv, char *channel)
{
	if (isupport(serv, "WHOX"))
		tcp_sendf (serv, "WHO %s %%ctnf,152\r\n", channel);
	else
		tcp_sendf (serv, "WHO %s\r\n", channel);
}

/*static void
irc_get_ip (server *serv, char *nick)
{
	tcp_sendf (serv, "WHO %s\r\n", nick);
}*/


/*
 *  Command: WHOIS
 *     Parameters: [<server>] <nickmask>[,<nickmask>[,...]]
 */
static void
irc_user_whois (server *serv, char *nicks)
{
	tcp_sendf (serv, "WHOIS %s\r\n", nicks);
}

static void
irc_message (server *serv, char *channel, char *text)
{
	char targetchan[CHANLEN];
	/* Make sure it's not a channel then check if the front session
	 * has whats needed for cmsg */
	if (isupport(serv, "CPRIVMSG") && find_ctarget(serv, targetchan, channel))
	{
		tcp_sendf (serv, "CPRIVMSG %s %s :%s\r\n", channel, 
				targetchan, text);
	}
	else
		tcp_sendf (serv, "PRIVMSG %s :%s\r\n", channel, text);
}

static void
irc_action (server *serv, char *channel, char *act)
{
	tcp_sendf (serv, "PRIVMSG %s :\001ACTION %s\001\r\n", channel, act);
}

static void
irc_notice (server *serv, char *channel, char *text)
{
	char targetchan[CHANLEN];
	/* Make sure it's not a channel then check if the front session
	 * has whats needed for cnotice */
	if (isupport(serv, "CNOTICE") && find_ctarget(serv, targetchan, channel))
	{
		tcp_sendf (serv, "CNOTICE %s %s :%s\r\n", channel, 
				targetchan, text);
	}
	else
		tcp_sendf (serv, "NOTICE %s :%s\r\n", channel, text);
}

static void
irc_topic (server *serv, char *channel, char *topic)
{
	if (!topic)
		tcp_sendf (serv, "TOPIC %s :\r\n", channel);
	else if (topic[0])
		tcp_sendf (serv, "TOPIC %s :%s\r\n", channel, topic);
	else
		tcp_sendf (serv, "TOPIC %s\r\n", channel);
}

static void
irc_list_channels (server *serv, char *arg)
{
	if (arg[0])
	{
		tcp_sendf (serv, "LIST %s\r\n", arg);
		return;
	}

	if (serv->use_listargs)
								/* 1234567890123456 */
		tcp_send_len (serv, "LIST >0,<10000\r\n", 16);
	else
		tcp_send_len (serv, "LIST\r\n", 6);
}

static void
irc_names (server *serv, char *channel)
{
	tcp_sendf (serv, "NAMES %s\r\n", channel);
}

static void
irc_change_nick (server *serv, char *new_nick)
{
	tcp_sendf (serv, "NICK %s\r\n", new_nick);
}

static void
irc_ping (server *serv, char *to, char *timestring)
{
	if (*to)
		tcp_sendf (serv, "PRIVMSG %s :\001PING %s\001\r\n", to, timestring);
	else
		tcp_sendf (serv, "PING %s\r\n", timestring);
}

static int
irc_raw (server *serv, char *raw)
{
	size_t len;
	char tbuf[4096];
	if (*raw)
	{
		len = strlen (raw);
		if (len < sizeof (tbuf) - 3)
		{
			len = snprintf (tbuf, sizeof (tbuf), "%s\r\n", raw);
			tcp_send_len (serv, tbuf, (int)len);
		} else
		{
			tcp_send_len (serv, raw, (int)len);
			tcp_send_len (serv, "\r\n", 2);
		}
		return TRUE;
	}
	return FALSE;
}

/* ============================================================== */
/* ======================= IRC INPUT ============================ */
/* ============================================================== */


static void
channel_date (rage_session *sess, char *chan, char *timestr)
{
	time_t timestamp = (time_t) atol (timestr);
	char *tim = ctime (&timestamp);
	tim[24] = 0;	/* get rid of the \n */
	EMIT_SIGNAL (XP_TE_CHANDATE, sess, chan, tim, NULL, NULL, 0);
}

/* Splits "buf" up into parv/parc */
void 
irc_split(server *serv,char *buf,int *parc,char *parv[])
{
	*parc=0;
	/* See if this starts with a server */
	if (serv) /* makes the code more generic */
	{
		if (*buf!=':')
			parv[(*parc)++]=serv->servername;
		else if (!*buf)
			return; /* Empty string?! */
		else
			buf++; /* Skip the initial : */
	}

	while (*parc<MAX_TOKENS) {
		/* Skip whitespace */
		while(*buf && *buf==' ') 
			buf++;

		/* Last word? */
		if (*buf==':') {
			parv[(*parc)++]=++buf;
			parv[(*parc)]="";
			return;
		}

		parv[(*parc)++]=buf;

		while(*buf && *buf!=' ')
			buf++;

		if (!*buf) {
			parv[(*parc)]="";
			return;
		}

		/* Null terminate this string */
		*(buf++)='\0';
	
	}

	/* If we got here, there were too many tokens */
	return;
}

/* 
 * parv[0] = sender prefix
 * parv[1] = numeric
 * parv[2] = destination
 * parv[3...] = args
 */
static void
irc_numeric(rage_session *sess, int parc, char *parv[])
{
	char line[512];
	rage_session *tmp = NULL;
	server *serv = sess->server;

	switch(atoi(parv[1])) {
		case RPL_WELCOME:  /* 001 */
			inbound_login_start(sess,parv[2],parv[0]);
			/* PTnet code not ported */
			break;
		case RPL_MYINFO: /* 004 */
			/* Don't bother trying to guess features,
			 * rely on RPL_ISUPPORT
			 */
			break;
		case RPL_ISUPPORT: /* 005 */
			inbound_005(sess->server,parc,parv);
			break;
		case 271: /* RPL_SILENCE ? */
			EMIT_SIGNAL(XP_TE_SILENCE, sess->server->server_session, 
					parv[3], NULL, NULL, NULL, 0);
			return;
		case 290: /* CAPAB reply */
		{
			int i;
			
			for (i = 3; i < parc; i++)
				dict_capab_insert(sess->server->isupport, parv[i]);
			break;
		}
		case RPL_AWAY: /* 301 */
			inbound_away(sess->server,parv[3],parv[4]);
			return;
		case RPL_USERHOST: /* 302 */
			/* Bletch this sucks */
			if (sess->server->skip_next_userhost)
			{
				char *eq = strchr(parv[3],'=');
				if (eq)
				{
					*eq='\0';
					if (!sess->server->p_cmp(parv[3],sess->server->nick)) {
						char *at = strrchr(eq+1,'@');
						if (at)
							inbound_foundip(sess,at+1);
					}
				}
				sess->server->skip_next_userhost=FALSE;
				return; /* Hide this */
			}
			break;
		case RPL_ISON:   /* 305 */
			notify_markonline(sess->server,parv);
			return;
		case RPL_UNAWAY: /* 306 */
			inbound_uback(sess->server);
			break;
		case RPL_NOWAWAY: /* 306 */
			inbound_uaway(sess->server);
			break;
		case RPL_WHOISSERVER: /* 312 */
			EMIT_SIGNAL(XP_TE_WHOIS3, 
					sess->server->server_session,
					parv[3],parv[4],
					NULL,
					NULL,
					0);
			return;
		case RPL_WHOISCHANNELS: /* 319 */
			EMIT_SIGNAL (XP_TE_WHOIS2, 
					sess->server->server_session,
					parv[3], parv[4], 
					NULL, 
					NULL, 
					0);
			return;
		case RPL_ENDOFWHO: /* 315 */
		{
			rage_session *who_sess;
			who_sess = find_channel(sess->server,parv[3]);

			if (who_sess)
			{
				if (!who_sess->doing_who)
					EMIT_SIGNAL(XP_TE_WHO, 
							sess->server->server_session,
							paste_parv(line, sizeof(line),
								3, parc, parv),
							NULL, NULL, NULL, 0);
				who_sess->doing_who = FALSE;
			} else
			{
				if (!sess->server->doing_dns)
					EMIT_SIGNAL (XP_TE_WHO, 
							sess->server->server_session,
							paste_parv(line, sizeof(line),
								3, parc, parv), 
							NULL, NULL, NULL, 0);
				sess->server->doing_dns = FALSE;
			}
			return;
		}
		case RPL_WHOISUSER:   /* 311 */
			sess->server->inside_whois = 1;
			/* FALL THRU */
		case RPL_WHOWASUSER:
			inbound_user_info_start(sess,parv[3]);
			EMIT_SIGNAL(XP_TE_WHOIS1, 
					sess->server->server_session,
					parv[3], parv[4], parv[5],
					parv[7], 0);
			return;
		case RPL_WHOISIDLE: /* 317 */
		{
			time_t timestamp = (time_t) atol(parv[5]);
			long idle = atol(parv[4]);
			char outbuf[64];
			char *tim;
			/* TODO: Add days support */
			snprintf(outbuf, sizeof(outbuf),
						"%02ld:%02ld:%02ld",
						idle/3600,
						(idle/60) % 60,
						idle % 60);
			if (timestamp == 0)
				EMIT_SIGNAL(XP_TE_WHOIS4, 
						sess->server->server_session,
						parv[3], outbuf, NULL,
						NULL, 0);
			else 
			{
				tim = ctime(&timestamp);
				tim[19] = '\0'; /* Get rid of the nasty \n */

				EMIT_SIGNAL (XP_TE_WHOIS4T, 
						sess->server->server_session,
						parv[3], outbuf, tim,
						NULL, 0);
			}
			return;
		}
		case RPL_WHOISOPERATOR: /* 313 */
		case 320: /* Is an identified user */
			EMIT_SIGNAL(XP_TE_WHOIS_ID, 
					sess->server->server_session,
					parv[3],parv[4], NULL, NULL, 0);
			return;
			
		case RPL_ENDOFWHOIS: /* 318 */
			sess->server->inside_whois = 0;
			EMIT_SIGNAL (XP_TE_WHOIS6, 
					sess->server->server_session,
					parv[3],NULL,
					NULL,NULL,0);
			return;

		case RPL_LISTSTART: /* 321 */
			if (!fe_is_chanwindow(sess->server)) 
				EMIT_SIGNAL (XP_TE_CHANLISTHEAD,
						sess->server->server_session,
						NULL,NULL,
						NULL,NULL,
						0);
			return;

		case RPL_LIST: /* 322 */
			if (fe_is_chanwindow (sess->server))
				fe_add_chan_list(sess->server,
						parv[3],parv[4],
						parv[5]);
			else
				EMIT_SIGNAL(XP_TE_CHANLIST, 
						sess->server->server_session,
						parv[3], parv[4],
						parv[5], NULL, 0);
			return;
		case RPL_LISTEND: /* 323 */
			if (!fe_is_chanwindow(sess->server))
				EMIT_SIGNAL(XP_TE_SERVTEXT, 
						sess->server->server_session, 
						parv[parc-1],
						parv[0], parv[1], NULL, 0);
			else
				fe_chan_list_end(sess->server);
			return;

		case RPL_CHANNELMODEIS: /* 324 */
		{
			char *chmode;
			sess = find_channel(sess->server,parv[3]);
			if (!sess)
				sess = serv->server_session;
			if (sess->ignore_mode)
				sess->ignore_mode = FALSE;
			else
				EMIT_SIGNAL(XP_TE_CHANMODES, sess, parv[3],
						parv[4], NULL, NULL, 0);
			/* TODO: use 005 to figure out which buttons to 
			 *       draw
			 */
			chmode = get_isupport(sess->server, "CHANMODES");
			while(*chmode)
			{
				if(*chmode != ',')
					fe_update_mode_buttons(sess, *chmode, '-');
				chmode++;
			}
			handle_mode(sess->server, parc, parv, "", TRUE); 
			return;
		}
		case RPL_CREATIONTIME: /* 329 */
			sess = find_channel(sess->server,parv[3]);
			if (sess)
			{
				if (sess->ignore_date)
					sess->ignore_date = FALSE;
				else
					channel_date(sess,parv[3],parv[4]);
			}
			return;
		case RPL_WHOISACCOUNT: /* 330 */
			EMIT_SIGNAL(XP_TE_WHOIS_AUTH, 
					sess->server->server_session,
					parv[3],parv[5], parv[4], NULL, 0);
			return;
		case RPL_TOPIC: /* 332 */
			inbound_topic(sess->server, parv[3], parv[4]);
			return;
		case RPL_TOPICWHOTIME:
			inbound_topictime(sess->server,
					parv[3],parv[4],atol(parv[5]));
			return;
		case RPL_WHOISACTUALLY: /* 338 */
			EMIT_SIGNAL (XP_TE_WHOIS_REALHOST, 
					sess->server->server_session, parv[3],
					parv[4], parv[5], parv[6], 0);
			return;
		case RPL_INVITING: /* 341 */
			EMIT_SIGNAL (XP_TE_UINVITE, sess, parv[3], parv[4],
					sess->server->servername, NULL, 0);
			return;
		case RPL_WHOREPLY: /* 352 */
		{	
			unsigned int away = 0;
			char *tmp;
			rage_session *who_sess = find_channel(sess->server, parv[3]);
			/* TODO: eww */
			if (*parv[8] == 'G')
				away = 1;

			if ((tmp = strchr(parv[9], ' ')))
			{
				tmp++;
				parv[9] = tmp;
			}
			parv[9] = strip_color(parv[9]);

			inbound_user_info(sess,parv[3],parv[4], parv[5], 
					parv[6], parv[7], parv[9], away);
			/* try to show only user initiated whos */

			if (!who_sess || !who_sess->doing_who)
				EMIT_SIGNAL(XP_TE_WHO, 
						sess->server->server_session, 
						paste_parv(line, sizeof(line),
							3, parc, parv), 
						NULL, NULL, NULL, 0);
			return;
		}

		case RPL_WHOSPCRPL: /* 354 */
			if (strcmp(parv[3],"152") == 0)
			{
				int away = 0;
				rage_session *who_sess = find_channel(sess->server, parv[4]);

				/* TODO: eew */
				if (*parv[6] == 'G')
					away = 1;

				inbound_user_info(sess, parv[4], 0, 0, 0, 
						parv[6], 0, away);

				if (!who_sess || !who_sess->doing_who)
					break;
				return;
			}
			else
				break;

		case RPL_NAMREPLY: /* 353 */
			inbound_nameslist (sess->server, parv[4],
					parv[5]);
			return;

		case RPL_ENDOFNAMES: /* 366 */
			if (!inbound_nameslist_end(sess->server, parv[3]))
				break;
			return;

		case RPL_BANLIST: /* 367 */
			inbound_banlist (sess, atol(parv[6]), parv[3], parv[4],
					parv[5]);
			return;

		case RPL_ENDOFBANLIST: /* 368 */
			sess = find_channel (sess->server, parv[3]);
			if (!sess)
				sess = serv->front_session;

			if (!fe_is_banwindow(sess))
				return;

			fe_ban_list_end(sess);
			break;

		case RPL_MOTD: /* 372 */
		case RPL_MOTDSTART: /* 375 */
			if (!prefs.skipmotd || sess->server->motd_skipped)
				EMIT_SIGNAL(XP_TE_MOTD, 
						sess->server->server_session,
						parv[parc-1], NULL, 
						NULL, NULL, 0);
			return;

		case RPL_ENDOFMOTD: /* 376 */
		case ERR_NOMOTD: /* 422 */
			run_005(sess->server);
			inbound_login_end(sess,parv[parc-1]);
			return;

		case ERR_NICKNAMEINUSE:
			if (sess->server->end_of_motd)
				break;
			inbound_next_nick(sess, parv[3]);
			return;

		case ERR_BANNICKCHANGE:
			if (sess->server->end_of_motd || is_channel (sess->server, parv[3]))
				break;
			inbound_next_nick(sess, parv[3]);
			break;

		case ERR_CHANNELISFULL: /* 471 */
			EMIT_SIGNAL(XP_TE_USERLIMIT, sess, parv[3], NULL, NULL,
					NULL, 0);
			break;

		case ERR_INVITEONLYCHAN: /* 473 */
			EMIT_SIGNAL(XP_TE_INVITE, sess, parv[3], NULL, NULL,
					NULL, );
			return;

		case ERR_BANNEDFROMCHAN: /* 474 */
			EMIT_SIGNAL(XP_TE_BANNED, sess, parv[3], NULL, NULL,
					NULL, 0);
			return;
		case ERR_BADCHANNELKEY: /* 475 */
			EMIT_SIGNAL(XP_TE_KEYWORD, sess, parv[3], NULL, NULL,
					NULL, 0);
			return;

		case 601: /* Log off */
			notify_set_offline(sess->server,parv[3], FALSE);
			return;
		case 605: /* Nowoff */
			notify_set_offline(sess->server,parv[3], TRUE);
			return;
		case 600: /* logon */
		case 604: /* newon */
			notify_set_online( sess->server,parv[3]);
			return;
	};
	/* TODO: Generate a signal based on the numeric, which gets rid of
	 *       a huge number of cases above, and lets people script things
	 *       far more easily.
	 *
	 * For the time being, the numeric is now in an extra parameter,
	 * which at least helps the cause.
	 */
	if (is_channel (sess->server, parv[2]))
	{
		tmp = find_channel(sess->server,parv[2]);
		if (!tmp)
			tmp = sess->server->server_session;
	} else {
		tmp = sess->server->server_session;
	}

	EMIT_SIGNAL(XP_TE_SERVTEXT, tmp, paste_parv(line, sizeof(line), 
				3, parc, parv), parv[0], parv[1], NULL, 0);
}

/* The fields are: level, weight, leak, limit and timestamp */
static throttle_t throttle_inv_data = { 0, 30, 10, 60, 0 }; /* max 3 invites during 10 seconds. */
#define throttle_invite gen_throttle(&throttle_inv_data)

static void 
irc_server(rage_session *sess, int parc, char *parv[])
{
	char *ex = strchr(parv[0],'!');
	int is_server;
	char nick[64]; 
	char ip[64];
	if (!ex) /* Hmm, server message */
	{
		safe_strcpy(ip, parv[0], sizeof(ip));
		safe_strcpy(nick, parv[0], sizeof(nick));
		is_server = 1;
	} else 
	{
		safe_strcpy(ip, ex+1, sizeof(ip));
		*ex='\0';
		safe_strcpy(nick,parv[0],sizeof(nick));
		*ex='!'; /* restore */
		is_server = 0;
	}

	switch(MAKE4UPPER(parv[1][0],parv[1][1],parv[1][2],parv[1][3])) {
		case M_INVITE:
			if (throttle_invite || ignore_check(parv[0], IG_INVI))
				return;
			/* TODO: Ratelimit invites to avoid floods */
			EMIT_SIGNAL(XP_TE_INVITED, sess, parv[3], nick,
					sess->server->servername, NULL, 0);
			return;

		case M_JOIN:
		{
			char *chan = parv[2];

			if (!sess->server->p_cmp(nick,sess->server->nick))
				inbound_ujoin(sess->server,chan,nick,ip);
			else
				inbound_join(sess->server,chan,nick,ip);
			return;
		}

		case M_MODE:
		{
			handle_mode (sess->server, parc,parv, nick, FALSE);
			return;
		}

		case M_NICK:
			inbound_newnick(sess->server,nick,parv[2],FALSE);
			return;

		case M_NOTICE:
		{
			int id = FALSE; /* identified */

			char *text;

			if (is_server) {
				inbound_notice(sess->server, parv[2],
						nick, parv[3], ip, is_server, id);
			} else {
				if (isupport(sess->server, "CAPAB-IDENTIFY-MSG"))
				{
					if (*parv[3] == '+')
					{
						id=TRUE;
						parv[3]++;
					} else if (*parv[3] == '-')
						parv[3]++;
				}
				text = parv[3];

				if (!ignore_check(parv[0], IG_NOTI))
					inbound_notice(sess->server, parv[2], 
							nick, text, ip, is_server, id);
			}
			return;
		}
		case M_PART:
		{
			char *chan = parv[2];
			char *reason = parv[3];

			/* If your nick matches exactly, then you parted */
			if (!strcmp(nick,sess->server->nick))
				inbound_upart(sess->server, chan, ip, reason);
			else
				inbound_part(sess->server, chan, nick, ip, reason);
			return;
		}
		case M_PRIVMSG:
		{
			char *to = parv[2];
			size_t len;
			int id = FALSE; /* identified */
			if (*to)
			{
				char *text;
				if (isupport(sess->server, "CAPAB-IDENTIFY-MSG"))
				{
					if (*parv[3] == '+')
					{
						id = TRUE;
						parv[3]++;
					} else if (*parv[3] == '-')
						parv[3]++;
				}
				text = parv[3];
				len = strlen(text);

				/* TODO: Not good enough */
				if (text[0] == '\001' && text[len-1]=='\001') 
				{
					text[len-1]=0;
					parv[3]++;
					/* DCC is handled in ctcp_handle aswell. */
					ctcp_handle(sess, to, nick, ip, parv[3], parc, parv);
				} else
				{
					if (is_channel(sess->server,to))
					{
						if (ignore_check(parv[0],IG_CHAN))
							return;
						inbound_chanmsg(sess->server, 
								NULL, 
								to,
								nick, 
								parv[parc-1],
								FALSE, 
								id);
					} else
					{
						if (ignore_check(parv[0], IG_PRIV))
							return;
						inbound_privmsg(sess->server, 
								nick, ip,
								parv[parc-1], 
								id);
					}
				}
			}
			break;
		}
		case M_PONG:
			inbound_ping_reply (sess->server->server_session, parv[3], parv[2]);
			return;
		case M_QUIT:
			inbound_quit(sess->server, nick, ip, parv[2]);
			return;
		case M_TOPIC:
			inbound_topicnew(sess->server,nick,parv[2],parv[3]);
			return;
		case M_KICK:
		{
			char *kicked = parv[3];
			char *reason = parv[4];
			if (*kicked) {
				if (!strcmp(kicked, sess->server->nick))
					inbound_ukick(sess->server,
							parv[2],nick,reason);
				else
					inbound_kick(sess->server,
							parv[2],kicked,
							nick,reason);
			}
			break;
		}
		case M_KILL:
			EMIT_SIGNAL(XP_TE_KILL, sess, nick, parv[4], 
					NULL, NULL, 0);
			break;
		case M_WALL:
			EMIT_SIGNAL(XP_TE_WALLOPS, sess, nick, parv[parc-1], 
					NULL, NULL, 0);
			break;
		case M_PING:
			tcp_sendf(sess->server, "PONG %s\r\n", parv[2]);
			break;
		case M_RPONG:
		{
			time_t tp;
			char line[5];
			/* parv[0] == source server
			 * parv[3] == dest server
			 * parv[4] == miliseconds
			 * parv[5] == user added time, ie from client.
			 */
			tp = time(NULL);
			snprintf (line, sizeof (line), "%li",  tp - atoi(parv[5]));
			EMIT_SIGNAL(XP_TE_RPONG, sess, parv[0], parv[3], parv[4], line, 0);
			break;
		}
		case M_ERROR:
			EMIT_SIGNAL(XP_TE_SERVERERROR, sess, parv[2], NULL,
					NULL, NULL, 0);
			break;
		case M_SILENCE:
			EMIT_SIGNAL(XP_TE_SILENCE, sess, parv[2], NULL, 
					NULL, NULL, 0);
			break;
		default:
		{
			char line[512];

			if (is_server)
			{
				paste_parv(line, sizeof(line), 3, parc, parv);
				EMIT_SIGNAL(XP_TE_SERVTEXT, sess, line,
						parv[0], parv[1], NULL, 0);
			}
			else
			{
				paste_parv(line, sizeof(line), 1, parc, parv);
				EMIT_SIGNAL(XP_TE_GARBAGE, sess, line,
						NULL, NULL, NULL, 0);
			}
		}
	}
}

static void
irc_inline (server *serv, char *buf, int len)
{
	rage_session *sess;
	char *parv[MAX_TOKENS];
	int parc;

	/* Split everything up into parc/parv */
	irc_split(serv,buf,&parc,parv);

	sess = serv->front_session;

	/* grab the server */
	if(plugin_emit_server (sess, parv[1], parc, parv))
		return;

	if(parc>1 && (parv[1][0]>='0' && parv[1][0]<='9'))
		irc_numeric(sess, parc, parv);
	else
		irc_server(sess, parc, parv);

	return;
}


void
proto_fill_her_up (server *serv)
{
	serv->p_inline = irc_inline;
	serv->p_invite = irc_invite;
	serv->p_cycle = irc_cycle;
	serv->p_ctcp = irc_ctcp;
	serv->p_nctcp = irc_nctcp;
	serv->p_quit = irc_quit;
	serv->p_kick = irc_kick;
	serv->p_part = irc_part;
	serv->p_nickserv = irc_nickserv;
	serv->p_join = irc_join;
	serv->p_login = irc_login;
	serv->p_join_info = irc_join_info;
	serv->p_mode = irc_mode;
	serv->p_user_list = irc_user_list;
	serv->p_away_status = irc_away_status;
	/*serv->p_get_ip = irc_get_ip;*/
	serv->p_whois = irc_user_whois;
	serv->p_get_ip = irc_user_list;
	serv->p_get_ip_uh = irc_userhost;
	serv->p_set_back = irc_set_back;
	serv->p_set_away = irc_set_away;
	serv->p_message = irc_message;
	serv->p_action = irc_action;
	serv->p_notice = irc_notice;
	serv->p_topic = irc_topic;
	serv->p_list_channels = irc_list_channels;
	serv->p_change_nick = irc_change_nick;
	serv->p_names = irc_names;
	serv->p_ping = irc_ping;
	serv->p_raw = irc_raw;
	serv->p_cmp = rfc_casecmp;	/* can be changed by 005 in modes.c */
}
