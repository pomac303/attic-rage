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

static void
ctcp_reply (session *sess, char *tbuf, char *nick, int parc, char *parv[],
				char *conf)
{
	conf = strdup (conf);
	/* process %C %B etc */
	check_special_chars (conf, TRUE);
	auto_insert (tbuf, 2048, conf, parc, parv, "", "", parv[4],"" ,"", nick);
	free (conf);
	handle_command (sess, tbuf, FALSE);
}

static int
ctcp_check (session *sess, char *tbuf, char *nick, int parc, char *parv[], 
		char *ctcp)
{
	int ret = 0;
	struct popup *pop;

	pop = (struct popup *) dict_find(ctcp_list, ctcp, &ret);

	if (ret)
		ctcp_reply (sess, tbuf, nick, parc, parv, pop->cmd);
	return ret;
}

/* The fields are: level, weight, leak, limit and timestamp */
static throttle_t throttle_data = { 0, 34, 10, 100, 0 };
#define ctcp_throttle gen_throttle(&throttle_data)

#define MAKE4(ch0, ch1, ch2, ch3)       (guint32)(ch0 | (ch1 << 8) | (ch2 << 16) | (ch3 << 24))

#define M_VERSION	MAKE4('V','E','R','S')
#define M_ACTION	MAKE4('A','C','T','I')
#define M_SOUND		MAKE4('S','O','U','N')

void
ctcp_handle (session *sess, char *to, char *nick,
				 char *msg, int parc, char *parv[])
{
	char *po;
	session *chansess;
	server *serv = sess->server;
	char outbuf[1024];
	guint32 type = MAKE4(toupper(parv[3][0]), toupper(parv[3][1]), 
			toupper(parv[3][2]), toupper(parv[3][3]));

	/* consider DCC and ACTION to be different from other CTCPs */
	if (type == M_ACTION)
	{
			inbound_action (sess, to, nick, msg + 7, FALSE);
			return;
	}
	else if(strncasecmp(parv[3], "DCC", 3) == 0)
	{
		/* but still let CTCP replies override DCCs */
		if (!ctcp_check (sess, outbuf, nick, parc, parv, parv[3] + 2))
		{
			if (!(ctcp_throttle || ignore_check (parv[0], IG_DCC)))
				handle_dcc (sess, nick, parc, parv);
		}
		return;
	}
	
	if (ctcp_throttle || ignore_check (parv[0], IG_CTCP))
		return;
	
	if(!ctcp_check (sess, outbuf, nick, parc, parv, parv[3]))
	{
		switch (type)
		{
			case M_VERSION:
				if (!prefs.hidever)
				{
					snprintf (outbuf, sizeof (outbuf), "VERSION Rage "VERSION" %s",
							get_cpu_str ());
					serv->p_nctcp (serv, nick, outbuf);
				}
				break;
			case M_SOUND:
				po = strchr (parv[4], '\001');
				if (po)
					po[0] = 0;
				EMIT_SIGNAL (XP_TE_CTCPSND, sess->server->front_session, parv[4],
								 nick, NULL, NULL, 0);
				snprintf (outbuf, sizeof (outbuf), "%s/%s", prefs.sounddir, parv[4]);
				if (strchr (parv[3], '/') == 0 && access (outbuf, R_OK) == 0)
				{
					snprintf (outbuf, sizeof (outbuf), "%s %s/%s", prefs.soundcmd,
								 prefs.sounddir, parv[4]);
					xchat_exec (outbuf);
				}
				return;
		}
	}

	po = strchr (msg, '\001');
	if (po)
		po[0] = 0;

	if (!is_channel (sess->server, to))
		EMIT_SIGNAL (XP_TE_CTCPGEN, sess->server->front_session, msg, nick,
						 NULL, NULL, 0);
	else
	{
		chansess = find_channel (sess->server, to);
		if (!chansess)
			chansess = sess;
		EMIT_SIGNAL (XP_TE_CTCPGENC, sess, msg, nick, to, NULL, 0);
	}
}
