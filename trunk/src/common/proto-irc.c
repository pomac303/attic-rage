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

dict_t rage_numerics_list = NULL;
dict_t rage_servermsgs_list = NULL;

static void
irc_login (server *serv, char *user, char *realname)
{
	char hostname[128];

	if (serv->password[0])
		send_commandf (serv, NULL, "PASS %s", serv->password);

	gethostname (hostname, sizeof (hostname) - 1);
	hostname[sizeof (hostname) - 1] = 0;
	if (hostname[0] == 0)
		strcpy (hostname, "0");

	send_commandf (serv, NULL, "NICK %s", serv->nick);
	send_commandf (serv, NULL, "USER %s %s %s :%s",
			user, hostname, serv->servername, realname);
}

static void
irc_nickserv (server *serv, char *pass)
{
	/* FIXME: this is a merge, but the code is fscking stupid! */
	switch (serv->nickservtype)
	{
		case 0:
			send_commandf (serv, "NickServ", 
					"PRIVMSG NickServ :identify %s", pass);
			break;
		case 1:
			send_commandf (serv, NULL, "NICKSERV :identify %s", pass);
			break;
		case 2:
			send_commandf (serv, NULL, "NS :identify %s", pass);
	}
}

static void
irc_join (server *serv, char *channel, char *key)
{
	if (key[0])
		send_commandf (serv, channel, "JOIN %s %s", channel, key);
	else
		send_commandf (serv, channel, "JOIN %s", channel);
}

static void
irc_part (server *serv, char *channel, char *reason)
{
	if (reason[0])
		send_commandf (serv, NULL, "PART %s :%s", channel, reason);
	else
		send_commandf (serv, NULL, "PART %s", channel);
}

static void
irc_quit (server *serv, char *reason)
{
	if (reason[0])
		send_commandf (serv, NULL, "QUIT :%s", reason);
	else
		send_command (serv, NULL, "QUIT");
}

static void
irc_set_back (server *serv)
{
	send_command (serv, NULL, "AWAY");
}

static void
irc_set_away (server *serv, char *reason)
{
	send_commandf (serv, NULL, "AWAY :%s", reason);
}

static void
irc_ctcp (server *serv, char *to, char *msg)
{
	send_messagef (serv, to, "\001%s\001", msg);
}

static void
irc_nctcp (server *serv, char *to, char *msg)
{
	send_replyf (serv, to, "\001%s\001", msg);
}

static void
irc_cycle (server *serv, char *channel, char *key)
{
	send_commandf (serv, NULL, "PART %s", channel);
	send_commandf (serv, NULL, "JOIN %s %s", channel, key);
}

static void
irc_kick (server *serv, char *channel, char *nick, char *reason)
{
	if (reason[0])
		send_commandf (serv, nick, "KICK %s %s :%s", channel, nick, reason);
	else
		send_commandf (serv, nick, "KICK %s %s", channel, nick);
}

static void
irc_invite (server *serv, char *channel, char *nick)
{
	send_commandf (serv, nick, "INVITE %s %s", nick, channel);
}

static void
irc_mode (server *serv, char *target, char *mode)
{
	send_mode(serv, target, mode);
}

/* find channel info when joined */

static void
irc_join_info (server *serv, char *channel)
{
	send_commandf (serv, channel, "MODE %s", channel);
}

/* initiate userlist retreival */

static void
irc_user_list (server *serv, char *channel)
{
	send_commandf (serv, channel, "WHO %s", channel);
}

/* userhost */

static void
irc_userhost (server *serv, char *nick)
{
	send_commandf (serv, nick, "USERHOST %s", nick);
}

static void
irc_away_status (server *serv, char *channel)
{
	if (isupport(serv, "WHOX"))
		send_commandf (serv, channel, "WHO %s %%ctnf,152", channel);
	else
		send_commandf (serv, channel, "WHO %s", channel);
}

/*static void
irc_get_ip (server *serv, char *nick)
{
	tcp__nomatch__sendf (serv, "WHO %s", nick);
}*/


/*
 *  Command: WHOIS
 *     Parameters: [<server>] <nickmask>[,<nickmask>[,...]]
 */
static void
irc_user_whois (server *serv, char *nicks)
{
	/* FIXME: should this be fixed in a better way? */
	send_commandf (serv, NULL, "WHOIS %s", nicks);
}

static void
irc_message (server *serv, char *channel, char *text)
{
	char targetchan[CHANLEN];
	/* Make sure it's not a channel then check if the front session
	 * has whats needed for cmsg */
	if (isupport(serv, "CPRIVMSG") && find_ctarget(serv, targetchan, channel))
		send_cmessagef (serv, channel, targetchan, "%s", text);
	else
		send_messagef (serv, channel, "%s", text);
}

static void
irc_action (server *serv, char *channel, char *act)
{
	send_messagef (serv, channel, "\001ACTION %s\001", act);
}

static void
irc_notice (server *serv, char *channel, char *text)
{
	char targetchan[CHANLEN];
	/* Make sure it's not a channel then check if the front session
	 * has whats needed for cnotice */
	if (isupport(serv, "CNOTICE") && find_ctarget(serv, targetchan, channel))
		send_cnoticef (serv, channel, targetchan, "%s", text);
	else
		send_noticef (serv, channel, "%s", text);
}

static void
irc_topic (server *serv, char *channel, char *topic)
{
	if (!topic) /* Used by fe-gtk to clear a topic. */
		send_commandf (serv, channel, "TOPIC %s :", channel);
	else if (topic[0])
		send_commandf (serv, channel, "TOPIC %s :%s", channel, topic);
	else
		send_commandf (serv, channel, "TOPIC %s", channel);
}

static void
irc_list_channels (server *serv, char *arg)
{
	if (arg[0])
	{
		send_commandf (serv, NULL, "LIST %s", arg);
		return;
	}

	if (serv->use_listargs)
								/* 1234567890123456 */
		send_command (serv, NULL, "LIST >0,<10000");
	else
		send_command (serv, NULL, "LIST");
}

static void
irc_names (server *serv, char *channel)
{
	send_commandf (serv, channel, "NAMES %s", channel);
}

static void
irc_change_nick (server *serv, char *new_nick)
{
	send_commandf (serv, NULL, "NICK %s", new_nick);
}

static void
irc_ping (server *serv, char *to, char *timestring)
{
	if (*to)
		send_messagef (serv, to, "\001PING %s\001", timestring);
	else
		send_commandf (serv, NULL, "PING %s", timestring);
}

static int
irc_raw (server *serv, char *raw)
{
	if (*raw)
	{
		send_command (serv, NULL, raw);
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

/* for numerics only */
#define PARSE_FUNC(FUNC) static int FUNC(rage_session *sess, int parc, char *parv[])

PARSE_FUNC(numeric_welcome)
{
	inbound_login_start(sess,parv[2],parv[0]);
	/* PTnet code not ported */
	return 0;
}

PARSE_FUNC(numeric_myinfo)
{
	/* Don't bother trying to guess features,
	 * rely on RPL_ISUPPORT */
	return 0;
}

PARSE_FUNC(numeric_isupport)
{
	inbound_005(sess->server,parc,parv);
	return 0;
}

PARSE_FUNC(numeric_bounce)
{
	char buf[800];
	PrintText(sess, "Warning, this code is untested. Please report the server that triggered it.\n");
	snprintf(buf, sizeof(buf), "Got bounced to %s:%s, due to: %s\n", parv[0], parv[1], parv[2]);
	PrintText(sess, buf);
	snprintf(buf, sizeof(buf), "server %s:%s", parv[0], parv[1]);
	handle_command(sess, buf, 0);
	return 1;
}

PARSE_FUNC(numeric_silence)
{
	EMIT_SIGNAL(XP_TE_SILENCE, sess->server->server_session, 
			parv[3], NULL, NULL, NULL, 0);
	return 1;
}

PARSE_FUNC(numeric_capab)
{
	int i;

	for (i = 3; i < parc; i++)
		dict_capab_insert(sess->server->isupport, parv[i]);
	return 0;
}

PARSE_FUNC(numeric_away)
{
	inbound_away(sess->server,parv[3],parv[4]);
	return 1;
}

PARSE_FUNC(numeric_userhost)
{
	/* Bletch this sucks */
	if (sess->server->skip_next_userhost)
	{
		char *eq = strchr(parv[3],'=');
		if (eq)
		{
			*eq='\0';
			if (!sess->server->p_cmp(parv[3],sess->server->nick))
			{
				char *at = strrchr(eq+1,'@');
				if (at)
					inbound_foundip(sess,at+1);
			}
		}
		sess->server->skip_next_userhost=FALSE;
		return 1; /* Hide this */
	}
	return 0;
}

PARSE_FUNC(numeric_ison)
{
	notify_markonline(sess->server, parv[3]);
	return 1;
}

PARSE_FUNC(numeric_unaway)
{
	inbound_uback(sess->server);
	return 0;
}

PARSE_FUNC(numeric_nowaway)
{
	inbound_uaway(sess->server);
	return 0;
}

PARSE_FUNC(numeric_whoisserver)
{
	EMIT_SIGNAL(XP_TE_WHOIS3, 
			sess->server->server_session,
			parv[3],parv[4],
			NULL,
			NULL,
			0);
	return 1;
}

PARSE_FUNC(numeric_whoischannels)
{
	EMIT_SIGNAL (XP_TE_WHOIS2, 
			sess->server->server_session,
			parv[3], parv[4], 
			NULL, 
			NULL, 
			0);
	return 1;
}

PARSE_FUNC(numeric_endofwho)
{
	rage_session *who_sess;
	char line[512];

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
	return 1;
}

PARSE_FUNC(numeric_whowasuser)
{
	inbound_user_info_start(sess,parv[3]);
	EMIT_SIGNAL(XP_TE_WHOIS1, 
			sess->server->server_session,
			parv[3], parv[4], parv[5],
			parv[7], 0);
	return 1;
}

PARSE_FUNC(numeric_whoisuser)
{
	sess->server->inside_whois = 1;
	return numeric_whowasuser(sess, parc, parv);
}

PARSE_FUNC(numeric_whoisidle)
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
	return 1;
}

PARSE_FUNC(numeric_whoisoperator)
{
	EMIT_SIGNAL(XP_TE_WHOIS_ID, 
			sess->server->server_session,
			parv[3],parv[4], NULL, NULL, 0);
	return 1;
}

PARSE_FUNC(numeric_endofwhois)
{
	sess->server->inside_whois = 0;
	EMIT_SIGNAL (XP_TE_WHOIS6, 
			sess->server->server_session,
			parv[3],NULL,
			NULL,NULL,0);
	return 1;
}

PARSE_FUNC(numeric_liststart)
{
	if (!fe_is_chanwindow(sess->server)) 
		EMIT_SIGNAL (XP_TE_CHANLISTHEAD,
				sess->server->server_session,
				NULL,NULL,
				NULL,NULL,
				0);
	return 1;
}

PARSE_FUNC(numeric_list)
{
	if (fe_is_chanwindow (sess->server))
		fe_add_chan_list(sess->server,
				parv[3],parv[4],
				parv[5]);
	else
		EMIT_SIGNAL(XP_TE_CHANLIST, 
				sess->server->server_session,
				parv[3], parv[4],
				parv[5], NULL, 0);
	return 1;
}

PARSE_FUNC(numeric_listend)
{
	if (!fe_is_chanwindow(sess->server))
		EMIT_SIGNAL(XP_TE_SERVTEXT, 
				sess->server->server_session, 
				parv[parc-1],
				parv[0], parv[1], NULL, 0);
	else
		fe_chan_list_end(sess->server);
	return 1;
}

PARSE_FUNC(numeric_channelmodeis)
{
	char *chmode;
	server *serv = sess->server;
	
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
	return 1;
}

PARSE_FUNC(numeric_creationtime)
{
	sess = find_channel(sess->server,parv[3]);
	if (sess)
	{
		if (sess->ignore_date)
			sess->ignore_date = FALSE;
		else
			channel_date(sess,parv[3],parv[4]);
	}
	return 1;
}

PARSE_FUNC(numeric_whoisaccount)
{
	EMIT_SIGNAL(XP_TE_WHOIS_AUTH, 
			sess->server->server_session,
			parv[3],parv[5], parv[4], NULL, 0);
	return 1;
}

PARSE_FUNC(numeric_topic)
{
	inbound_topic(sess->server, parv[3], parv[4]);
	return 1;
}

PARSE_FUNC(numeric_topicwhotime)
{
	inbound_topictime(sess->server,
			parv[3],parv[4],atol(parv[5]));
	return 1;
}

PARSE_FUNC(numeric_whoisactually)
{
	EMIT_SIGNAL (XP_TE_WHOIS_REALHOST, 
			sess->server->server_session, parv[3],
			parv[4], parv[5], parv[6], 0);
	return 1;
}

PARSE_FUNC(numeric_inviting)
{
	EMIT_SIGNAL (XP_TE_UINVITE, sess, parv[3], parv[4],
			sess->server->servername, NULL, 0);
	return 1;
}

/*
PARSE_FUNC(numeric_exemptlist)
{
	//inbound_banlist (sess, atol (parv[6]), parv[3], parv[4], parv[5], TRUE)
	return 0;
}

PARSE_FUNC(numeric_endofexemptlist)
{
	server *serv = sess->server;

	sess = find_channel (serv, parv[3]);
	if (!sess)
		sess = serv->front_session;
	if (!fe_is_banwindow (sess))
		return 1;
	fe_ban_list_end (sess);
	return 0;
}
*/
PARSE_FUNC(numeric_whoreply)
{
	unsigned int away = 0;
	char line[512];
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

	parv[9] = dstr_strip_color(parv[9]);

	inbound_user_info(sess,parv[3],parv[4], parv[5], 
			parv[6], parv[7], parv[9], away);
	
	/* try to show only user initiated whos */

	if (!who_sess || !who_sess->doing_who)
		EMIT_SIGNAL(XP_TE_WHO, 
				sess->server->server_session, 
				paste_parv(line, sizeof(line),
					3, parc, parv), 
				NULL, NULL, NULL, 0);
	return 1;
}

PARSE_FUNC(numeric_whospcrpl)
{
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
			return 0;
		return 1;
	}
	else
		return 0;
}

PARSE_FUNC(numeric_namreply)
{
	inbound_nameslist (sess->server, parv[4],
			parv[5]);
	return 1;
}

PARSE_FUNC(numeric_endofnames)
{
	if (!inbound_nameslist_end(sess->server, parv[3]))
		return 0;
	return 1;
}

PARSE_FUNC(numeric_banlist)
{
	inbound_banlist (sess, atol(parv[6]), parv[3], parv[4],
			parv[5]);
	return 1;
}

PARSE_FUNC(numeric_endofbanlist)
{
	server *serv = sess->server;

	sess = find_channel (sess->server, parv[3]);
	if (!sess)
		sess = serv->front_session;

	if (!fe_is_banwindow(sess))
		return 1;

	fe_ban_list_end(sess);
	return 0;
}

PARSE_FUNC(numeric_motd)
{
	if (!prefs.skipmotd || sess->server->motd_skipped)
		EMIT_SIGNAL(XP_TE_MOTD, 
				sess->server->server_session,
				parv[parc-1], NULL, 
				NULL, NULL, 0);
	return 1;
}

PARSE_FUNC(numeric_endofmotd)
{
	run_005(sess->server);
	inbound_login_end(sess,parv[parc-1]);
	return 1;
}

PARSE_FUNC(numeric_nosuchtarget)
{
	queue_remove_target(sess->server, parv[3], 0);
	return 0;
}

PARSE_FUNC(numeric_nicknameinuse)
{
	if (sess->server->end_of_motd)
		return 0;
	inbound_next_nick(sess, parv[3]);
	return 1;
}

PARSE_FUNC(numeric_bannickchange)
{
	if (sess->server->end_of_motd || is_channel (sess->server, parv[3]))
		return 0;
	inbound_next_nick(sess, parv[3]);
	return 0; /* XXX: shouldn't this be 1? */
}

PARSE_FUNC(numeric_channelisfull)
{
	EMIT_SIGNAL(XP_TE_USERLIMIT, sess, parv[3], NULL, NULL,
			NULL, 0);
	return 0;
}

PARSE_FUNC(numeric_inviteonlychan)
{
	EMIT_SIGNAL(XP_TE_INVITE, sess, parv[3], NULL, NULL,
			NULL, );
	return 1;
}

PARSE_FUNC(numeric_bannedfromchan)
{
	EMIT_SIGNAL(XP_TE_BANNED, sess, parv[3], NULL, NULL,
			NULL, 0);
	return 1;
}

PARSE_FUNC(numeric_badchannelkey)
{
	EMIT_SIGNAL(XP_TE_KEYWORD, sess, parv[3], NULL, NULL,
			NULL, 0);
	return 1;
}

PARSE_FUNC(numeric_6XX_logoff)
{
	notify_set_offline(sess->server,parv[3], FALSE);
	return 1;
}

PARSE_FUNC(numeric_6XX_nowoff)
{
	notify_set_offline(sess->server,parv[3], TRUE);
	return 1;
}

PARSE_FUNC(numeric_6XX_logon)
{
	notify_set_online( sess->server,parv[3]);
	return 1;
}

#undef PARSE_FUNC /* for numerics only */

static ircparser_numeric irc_numerics[] = {
	{RPL_WELCOME, numeric_welcome},			/* 001 */
	{RPL_MYINFO, numeric_myinfo},			/* 004 */
	{RPL_ISUPPORT, numeric_isupport},		/* 005 */
	{10, numeric_bounce},				/* RPL_BOUNCE */
	{271, numeric_silence},				/* RPL_SILENCE ? */
	{290, numeric_capab},				/* RPL_CAPAB */
	{RPL_AWAY, numeric_away},			/* 301 */
	{RPL_USERHOST, numeric_userhost},		/* 302 */
	{RPL_ISON, numeric_ison},			/* 303 */
	{RPL_UNAWAY, numeric_unaway},			/* 305 */
	{RPL_NOWAWAY, numeric_nowaway},			/* 306 */
	{RPL_WHOISSERVER, numeric_whoisserver},		/* 312 */
	{RPL_WHOISCHANNELS, numeric_whoischannels}, 	/* 319 */
	{RPL_ENDOFWHO, numeric_endofwho}, 		/* 315 */
	{RPL_WHOISUSER, numeric_whoisuser},		/* 311 */
	{RPL_WHOWASUSER, numeric_whowasuser},		/* 314 */
	{RPL_WHOISIDLE, numeric_whoisidle},		/* 317 */
	{RPL_WHOISOPERATOR, numeric_whoisoperator}, 	/* 313 */
	{320, numeric_whoisoperator},			/* RPL_LOGEDINAS ? */
	{RPL_ENDOFWHOIS, numeric_endofwhois},		/* 318 */
	{RPL_LISTSTART, numeric_liststart},		/* 321 */
	{RPL_LIST, numeric_list},			/* 322 */
	{RPL_LISTEND, numeric_listend},			/* 323 */
	{RPL_CHANNELMODEIS, numeric_channelmodeis}, 	/* 324 */
	{RPL_CREATIONTIME, numeric_creationtime},	/* 329 */
	{RPL_WHOISACCOUNT, numeric_whoisaccount},	/* 330 */
	{RPL_TOPIC, numeric_topic},			/* 332 */
	{RPL_TOPICWHOTIME, numeric_topicwhotime},	/* 333 */
	{RPL_WHOISACTUALLY, numeric_whoisactually}, 	/* 338 */
	{RPL_INVITING, numeric_inviting},		/* 341 */
//	{348, numeric_exemptlist},			/* RPL_EXEMPTLIST */
//	{349, numeric_endofexemptlist},			/* RPL_ENDOFEXEMPTLIST */
	{RPL_WHOREPLY, numeric_whoreply},		/* 352 */
	{RPL_WHOSPCRPL, numeric_whospcrpl},		/* 354, WHOX */
	{RPL_NAMREPLY, numeric_namreply},		/* 353 */
	{RPL_ENDOFNAMES, numeric_endofnames},		/* 366 */
	{RPL_BANLIST, numeric_banlist},			/* 367 */
	{RPL_ENDOFBANLIST, numeric_endofbanlist},	/* 368 */
	{RPL_MOTD, numeric_motd},			/* 372 */
	{RPL_MOTDSTART, numeric_motd},			/* 375 */
	{RPL_ENDOFMOTD, numeric_endofmotd}, 		/* 376 */
	{ERR_NOSUCHNICK, numeric_nosuchtarget},		/* 401 */
	{ERR_NOSUCHCHANNEL, numeric_nosuchtarget},	/* 403 */
	{ERR_CANNOTSENDTOCHAN, numeric_nosuchtarget},	/* 404 */
	{ERR_NOMOTD, numeric_endofmotd},		/* 422 */
	{ERR_NICKNAMEINUSE, numeric_nicknameinuse},	/* 433 */
	{ERR_BANNICKCHANGE, numeric_bannickchange},	/* 437 */
	{ERR_CHANNELISFULL, numeric_channelisfull},	/* 471 */
	{ERR_INVITEONLYCHAN, numeric_inviteonlychan},	/* 473 */
	{ERR_BANNEDFROMCHAN, numeric_bannedfromchan},	/* 474 */
	{ERR_BADCHANNELKEY, numeric_badchannelkey},	/* 475 */
	{601, numeric_6XX_logoff},			/* Log off */
	{605, numeric_6XX_nowoff},			/* Now off */
	{600, numeric_6XX_logon},			/* Log on */
	{604, numeric_6XX_logon},			/* New on */
};

/* The fields are: level, weight, leak, limit and timestamp */
static throttle_t throttle_inv_data = { 0, 30, 10, 60, 0 }; /* max 3 invites during 10 seconds. */
#define throttle_invite gen_throttle(&throttle_inv_data)

/* servermsg only */
#define PARSE_FUNC(FUNC) static int FUNC(rage_session *sess, int parc, \
		char *parv[], char *ip, char *nick, int is_server)

PARSE_FUNC(servermsg_invite)
{
	if (throttle_invite || ignore_check(parv[0], IG_INVI))
		return 1;
	/* TODO: Ratelimit invites to avoid floods */
	EMIT_SIGNAL(XP_TE_INVITED, sess, parv[3], nick,
			sess->server->servername, NULL, 0);
	return 1;
}

PARSE_FUNC(servermsg_join)
{
	char *chan = parv[2];

	if (!sess->server->p_cmp(nick,sess->server->nick))
		inbound_ujoin(sess->server,chan,nick,ip);
	else
		inbound_join(sess->server,chan,nick,ip);
	return 1;
}

PARSE_FUNC(servermsg_mode)
{
	handle_mode (sess->server, parc,parv, nick, FALSE);
	return 1;
}

PARSE_FUNC(servermsg_nick)
{
	inbound_newnick(sess->server,nick,parv[2],FALSE);
	queue_target_change(sess->server, nick, parv[2]);
	return 1;
}

PARSE_FUNC(servermsg_notice)
{
	int id = FALSE; /* identified */

	char *text;

	if (is_server)
		inbound_notice(sess->server, parv[2],
				nick, parv[3], ip, is_server, id);
	else
	{
		if (isupport(sess->server, "CAPAB-IDENTIFY-MSG"))
		{
			if (*parv[3] == '+')
			{
				id=TRUE;
				parv[3]++;
			} 
			else if (*parv[3] == '-')
				parv[3]++;
		}
		text = parv[3];

		if (!ignore_check(parv[0], IG_NOTI))
			inbound_notice(sess->server, parv[2], 
					nick, text, ip, is_server, id);
	}
	return 1;
}

PARSE_FUNC(servermsg_part)
{
	char *chan = parv[2];
	char *reason = parv[3];

	/* If your nick matches exactly, then you parted */
	if (!strcmp(nick,sess->server->nick))
		inbound_upart(sess->server, chan, ip, reason);
	else
		inbound_part(sess->server, chan, nick, ip, reason);
	return 1;
}

PARSE_FUNC(servermsg_privmsg)
{
	char *to = parv[2], *p, *q, buf[512];
	int id = FALSE, done_ctcp = 0; /* identified */
	if (*to)
	{
		char *text;
		if (isupport(sess->server, "CAPAB-IDENTIFY-MSG"))
		{
			if (*parv[3] == '+')
			{
				id = TRUE;
				parv[3]++;
			} 
			else if (*parv[3] == '-')
				parv[3]++;
		}
		
		text = parv[3];
		buf[0] = 0;

		while (*text && (p = strchr(text, '\001')))
		{
			if ((q = strchr(p+1, '\001')))
			{
				*p++ = 0;
				*q++ = 0;
				strncat(buf, text, sizeof(buf)-1);
				ctcp_handle(sess, to, nick, parv[0], p);
				text = q;
				done_ctcp = 1;
			}
			else
				break;
		}
		if (done_ctcp)
		{
			if (*text)
			{
				strncat(buf, text, sizeof(buf)-1);
				text = buf;
			}
			else
				return 0;
		}	
		if (is_channel(sess->server, to))
		{
			if (ignore_check(parv[0], IG_CHAN))
				return 1;
			inbound_chanmsg(sess->server, NULL, 
					to, nick, text, FALSE, id);
		}
		else
		{
			if (ignore_check(parv[0], IG_PRIV))
				return 1;
			inbound_privmsg(sess->server, 
					nick, ip, text, id);
		}
	}
	return 0;
}

PARSE_FUNC(servermsg_pong)
{
	inbound_ping_reply (sess->server->server_session, parv[3], parv[2]);
	return 1;
}

PARSE_FUNC(servermsg_quit)
{
	inbound_quit(sess->server, nick, ip, parv[2]);
	queue_remove_target(sess->server, nick, 0);
	return 1;
}

PARSE_FUNC(servermsg_topic)
{
	inbound_topicnew(sess->server,nick,parv[2],parv[3]);
	return 1;
}

PARSE_FUNC(servermsg_kick)
{
	char *kicked = parv[3];
	char *reason = parv[4];
	if (*kicked)
	{
		if (!strcmp(kicked, sess->server->nick))
			inbound_ukick(sess->server, parv[2],nick,reason);
		else
			inbound_kick(sess->server, parv[2],kicked, nick,reason);
	}
	return 0;
}

PARSE_FUNC(servermsg_kill)
{
	EMIT_SIGNAL(XP_TE_KILL, sess, nick, parv[4], 
			NULL, NULL, 0);
	return 0;
}

PARSE_FUNC(servermsg_wallops)
{
	EMIT_SIGNAL(XP_TE_WALLOPS, sess, nick, parv[parc-1], 
			NULL, NULL, 0);
	return 0;
}

PARSE_FUNC(servermsg_ping)
{
	send_commandf (sess->server, NULL, "PONG %s", parv[2]);
	return 0;
}

PARSE_FUNC(servermsg_rpong)
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
	return 0;
}

PARSE_FUNC(servermsg_error)
{
	EMIT_SIGNAL(XP_TE_SERVERERROR, sess, parv[2], NULL,
			NULL, NULL, 0);
	return 0;
}

PARSE_FUNC(servermsg_silence)
{
	EMIT_SIGNAL(XP_TE_SILENCE, sess, parv[2], NULL, 
			NULL, NULL, 0);
	return 0;
}

#undef PARSE_FUNC /* servermsg only */

static struct ircparser_server irc_servermsgs[] = {
	{"INVITE", servermsg_invite},
	{"JOIN", servermsg_join},
	{"MODE", servermsg_mode},
	{"NICK", servermsg_nick},
	{"NOTICE", servermsg_notice},
	{"PART", servermsg_part},
	{"PRIVMSG", servermsg_privmsg},
	{"PONG", servermsg_pong},
	{"QUIT", servermsg_quit},
	{"TOPIC", servermsg_topic},
	{"KICK", servermsg_kick},
	{"KILL", servermsg_kill},
	{"WALLOPS", servermsg_wallops},
	{"PING", servermsg_ping},
	{"RPONG", servermsg_rpong},
	{"ERROR", servermsg_error},
	{"SILENCE", servermsg_silence},
};

void
setup_parser(void)
{
	int i;
	int nbr_elem = sizeof(irc_numerics) / sizeof(ircparser_numeric);

	rage_numerics_list = dict_numeric_new();
	rage_servermsgs_list = dict_new();

	for (i = 0; i < nbr_elem; i++)
		dict_numeric_insert(rage_numerics_list,
				&irc_numerics[i].numeric, &irc_numerics[i]);

	nbr_elem = sizeof(irc_servermsgs) / sizeof(ircparser_server);

	for (i = 0; i < nbr_elem; i++)
		dict_cmd_insert(rage_servermsgs_list,
				irc_servermsgs[i].name, &irc_servermsgs[i]);
}

static void
irc_inline (server *serv, char *buf, int len)
{
	rage_session *sess, *tmp;
	char *parv[MAX_TOKENS];
	int parc, found;
	char line[512];

	/* Split everything up into parc/parv */
	irc_split(serv,buf,&parc,parv);

	sess = serv->front_session;

	/* grab the server */
	if(plugin_emit_server (sess, parv[1], parc, parv))
		return;

	/* parv[0] = sender prefix
	 * parv[1] = numeric
	 * parv[2] = destination
	 * parv[3...] = args */
	if(parc>1 && isdigit(*parv[1]))
	{
		int numeric;
		ircparser_numeric *parse;
		
		numeric = atoi(parv[1]);
		parse = dict_numeric_find(rage_numerics_list, &numeric, &found);
		if (found)
			found = parse->callback(sess, parc, parv);

		if (!found)
		{
			if (is_channel (sess->server, parv[2]))
			{
				tmp = find_channel(sess->server,parv[2]);
				if (!tmp)
					tmp = sess->server->server_session;
			} 
			else
				tmp = sess->server->server_session;
			EMIT_SIGNAL(XP_TE_SERVTEXT, tmp, paste_parv(line, sizeof(line),
						3, parc, parv), parv[0], parv[1], NULL, 0);
		}
	}
	else
	{
		char *ex = strchr(parv[0],'!');
		int is_server;
		char nick[64];
		char ip[64];
		ircparser_server *parse;
		
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
		
		parse = dict_find(rage_servermsgs_list, parv[1], &found);
		if (found)
			found = parse->callback(sess, parc, parv, ip, nick, is_server);
		else
		{
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
