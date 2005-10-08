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

#define CTCP_LEN 512

static void
ctcp_reply (rage_session *sess, char *tbuf, char *nick, int parc, char *parv[],
				char *conf)
{
	conf = strdup (conf);
	/* process %C %B etc */
	check_special_chars (conf, TRUE);
	auto_insert (tbuf, CTCP_LEN, conf, parc, parv, "", "", parv[1],
					"" ,"", nick);
	free (conf);
	handle_command (sess, tbuf, FALSE);
}

/* The fields are: level, weight, leak, limit and timestamp */
static throttle_t ctcp_throttle_data = { 0, 34, 10, 100, 0 };
#define ctcp_throttle gen_throttle(&ctcp_throttle_data)
static throttle_t dcc_throttle_data = { 0, 34, 20, 100, 0};
#define dcc_throttle gen_throttle(&dcc_throttle_data)

void
ctcp_handle (rage_session *sess, char *to, char *nick, char *host, char *msg)
{
	rage_session *chansess;
	server *serv = sess->server;
	char outbuf[CTCP_LEN], buf[CTCP_LEN];
	char *parv[MAX_TOKENS], *arg;
	int parc, ret = 0;
	struct popup *pop;

	/* consider DCC and ACTION to be different from other CTCPs */
	if (strncasecmp("ACTION", msg, 4) == 0)
	{
			inbound_action (sess, to, nick, msg + 7, FALSE);
			return;
	}

	flood_check(nick, host, sess->server, sess, 0);

	if ((arg = strchr(msg, ' ')))
		arg++;
	
	if (strncasecmp("DCC ", msg, 3) == 0)
	{
		/* we don't allow dcc overrides.*/
		if (!(dcc_throttle || ignore_check (host, IG_DCC)))
			handle_dcc (sess, nick, arg);
		return;
	}
	
	if (ctcp_throttle || ignore_check (host, IG_CTCP))
		return;

	strcpy(buf, msg); /* msg should always be less than 512 chars */
	split_cmd_parv(buf, &parc, parv);
	
	pop = (struct popup *) dict_find(ctcp_list, parv[0], &ret);
	if (ret)
		ctcp_reply (sess, outbuf, nick, parc, parv, pop->cmd);
	else
	{
		if ((strncasecmp("VERSION", msg, 4) == 0))
		{
			if (!prefs.hidever)
			{
				snprintf (outbuf, sizeof (outbuf), "VERSION Rage "VERSION"-%s %s",
						rage_svn_version, get_cpu_str ());
				serv->p_nctcp (serv, nick, outbuf);
			}
		}
		else if ((strncasecmp("SOUND", msg, 4) == 0))
		{
			EMIT_SIGNAL (XP_TE_CTCPSND, sess->server->front_session, parv[1],
					nick, NULL, NULL, 0);
			if (strchr(parv[1], '/'))
			{
				snprintf (outbuf, sizeof (outbuf), "%s/%s", prefs.sounddir, parv[1]);
				if (access (outbuf, R_OK) == 0)
				{
					snprintf (outbuf, sizeof (outbuf), "%s %s/%s", prefs.soundcmd,
							prefs.sounddir, parv[4]);
					xchat_exec (outbuf);
				}
			}
			return;
		}
	}

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
