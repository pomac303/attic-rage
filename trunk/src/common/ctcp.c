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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "xchat.h"
#include "cfgfiles.h"
#include "util.h"
#include "modes.h"
#include "outbound.h"
#include "ignore.h"
#include "inbound.h"
#include "dcc.h"
#include "text.h"
#include "ctcp.h"
#include "server.h"
#include "xchatc.h"


static void
ctcp_reply (session *sess, char *tbuf, char *nick, int parc, char *parv[],
				char *conf)
{
	conf = strdup (conf);
	/* process %C %B etc */
	check_special_chars (conf, TRUE);
	auto_insert (tbuf, 2048, conf, parc,parv, "","", parv[5], "","", nick);
	free (conf);
	handle_command (sess, tbuf, FALSE);
}

static int
ctcp_check (session *sess, char *tbuf, char *nick, int parc, char *parv[], 
		char *ctcp)
{
	int ret = 0;
	char *po;
	struct popup *pop;
	GSList *list = ctcp_list;

	po = strchr (ctcp, '\001');
	if (po)
		*po = 0;

	po = strchr (parv[4], '\001');
	if (po)
		*po = 0;

	while (list)
	{
		pop = (struct popup *) list->data;
		if (!strcasecmp (ctcp, pop->name))
		{
			ctcp_reply (sess, tbuf, nick, parc, parv, pop->cmd);
			ret = 1;
		}
		list = list->next;
	}
	return ret;
}

void
ctcp_handle (session *sess, char *to, char *nick,
				 char *msg, int parc, char *parv[])
{
	char *po;
	session *chansess;
	server *serv = sess->server;
	char outbuf[1024];

	/* consider DCC to be different from other CTCPs */
	if (!strncasecmp (msg, "DCC", 3))
	{
		/* but still let CTCP replies override it */
		if (!ctcp_check (sess, outbuf, nick, parc, parv, parv[3] + 2))
		{
			if (!ignore_check (parv[0], IG_DCC))
				handle_dcc (sess, nick, parc, parv);
		}
		return;
	}

	if (ignore_check (parv[0], IG_CTCP))
		return;

	if (!strcasecmp (msg, "VERSION") && !prefs.hidever)
	{
		snprintf (outbuf, sizeof (outbuf), "VERSION Rage "VERSION" %s",
					 get_cpu_str ());
		serv->p_nctcp (serv, nick, outbuf);
	}

	if (!ctcp_check (sess, outbuf, nick, parc, parv, parv[3] + 2))
	{
		if (!strncasecmp (msg, "ACTION", 6))
		{
			inbound_action (sess, to, nick, msg + 7, FALSE);
			return;
		}
		if (!strncasecmp (msg, "SOUND", 5))
		{
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
	{
		EMIT_SIGNAL (XP_TE_CTCPGEN, sess->server->front_session, msg, nick,
						 NULL, NULL, 0);
	} else
	{
		chansess = find_channel (sess->server, to);
		if (!chansess)
			chansess = sess;
		EMIT_SIGNAL (XP_TE_CTCPGENC, sess, msg, nick, to, NULL, 0);
	}
}
