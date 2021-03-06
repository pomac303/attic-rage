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
 *
 * Wayne Conrad, 3 Apr 1999: Color-coded DCC file transfer status windows
 * Bernhard Valenti <bernhard.valenti@gmx.net> 2000-11-21: Fixed DCC send behind nat
 *
 * 2001-03-08 Added support for getting "dcc_ip" config parameter.
 * Jim Seymour (jseymour@LinxNet.com)
 */

#include "rage.h"
#include <inttypes.h>

static char *dcctypes[] = { "SEND", "RECV", "CHAT", "CHAT" };

struct dccstat_info dccstat[] = {
	{N_("Waiting"), 1 /*black */ },
	{N_("Active"), 12 /*cyan */ },
	{N_("Failed"), 4 /*red */ },
	{N_("Done"), 3 /*green */ },
	{N_("Connect"), 1 /*black */ },
	{N_("Aborted"), 4 /*red */ },
};

static int dcc_global_throttle;	/* 0x1 = sends, 0x2 = gets */
static int dcc_sendcpssum, dcc_getcpssum;

static struct DCC *new_dcc (void);
static gboolean dcc_send_data (GIOChannel *, GIOCondition, struct DCC *);
static gboolean dcc_read (GIOChannel *, GIOCondition, struct DCC *);
static gboolean dcc_read_ack (GIOChannel *source, GIOCondition condition, struct DCC *dcc);

static int new_id(void)
{
	static int id = 1;
	return id++;
}

static double
timeval_diff (GTimeVal *greater,
				 GTimeVal *less)
{
	long usecdiff;
	double result;
	
	result = greater->tv_sec - less->tv_sec;
	usecdiff = (long) greater->tv_usec - less->tv_usec;
	result += (double) usecdiff / 1000000;
	
	return result;
}

static void
dcc_unthrottle (struct DCC *dcc)
{
	/* don't unthrottle here, but delegate to funcs */
	if (dcc->type == TYPE_RECV)
		dcc_read (NULL, 0, dcc);
	else
		dcc_send_data (NULL, 0, dcc);
}

static void
dcc_calc_cps (struct DCC *dcc)
{
	GTimeVal now;
	int oldcps;
	double timediff, startdiff;
	int glob_throttle_bit, wasthrottled;
	int *cpssum, glob_limit;
	off_t pos, posdiff;

	g_get_current_time (&now);

	/* the pos we use for sends is an average
		between pos and ack */
	if (dcc->type == TYPE_SEND)
	{
		/* carefull to avoid 32bit overflow */
		pos = dcc->pos - ((dcc->pos - dcc->ack) / 2);
		glob_throttle_bit = 0x1;
		cpssum = &dcc_sendcpssum;
		glob_limit = prefs.dcc_global_max_send_cps;
	}
	else
	{
		pos = dcc->pos;
		glob_throttle_bit = 0x2;
		cpssum = &dcc_getcpssum;
		glob_limit = prefs.dcc_global_max_get_cps;
	}

	if (!dcc->firstcpstv.tv_sec && !dcc->firstcpstv.tv_usec)
		dcc->firstcpstv = now;
	else
	{
		startdiff = timeval_diff (&now, &dcc->firstcpstv);
		if (startdiff < 1)
			startdiff = 1;
		else if (startdiff > CPS_AVG_WINDOW)
			startdiff = CPS_AVG_WINDOW;

		timediff = timeval_diff (&now, &dcc->lastcpstv);
		if (timediff > startdiff)
			timediff = startdiff = 1;

		posdiff = pos - dcc->lastcpspos;
		oldcps = dcc->cps;
		dcc->cps = (int)((posdiff / timediff) * (timediff / startdiff)
			+ (dcc->cps * (1 - (timediff / startdiff))));

		*cpssum += dcc->cps - oldcps;
	}

	dcc->lastcpspos = pos;
	dcc->lastcpstv = now;

	/* now check cps against set limits... */
	wasthrottled = dcc->throttled;

	/* check global limits first */
	dcc->throttled &= ~0x2;
	if (glob_limit > 0 && *cpssum >= glob_limit)
	{
		dcc_global_throttle |= glob_throttle_bit;
		if (dcc->maxcps >= 0)
			dcc->throttled |= 0x2;
	}
	else
		dcc_global_throttle &= ~glob_throttle_bit;

	/* now check per-connection limit */
	if (dcc->maxcps > 0 && dcc->cps > dcc->maxcps)
		dcc->throttled |= 0x1;
	else
		dcc->throttled &= ~0x1;

	/* take action */
	if (wasthrottled && !dcc->throttled)
		dcc_unthrottle (dcc);
}

static void
dcc_remove_from_sum (struct DCC *dcc)
{
	if (dcc->dccstat != STAT_ACTIVE)
		return;
	if (dcc->type == TYPE_SEND)
		dcc_sendcpssum -= dcc->cps;
	else if (dcc->type == TYPE_RECV)
		dcc_getcpssum -= dcc->cps;
}

/* this is called from xchat.c:xchat_misc_checks() every 2 seconds. */

void
dcc_check_timeouts (void)
{
	struct DCC *dcc;
	time_t tim = time (NULL);
	GSList *next, *list = dcc_list;

	while (list)
	{
		dcc = (struct DCC *) list->data;
		next = list->next;

		switch (dcc->dccstat)
		{
		case STAT_ACTIVE:
			dcc_calc_cps (dcc);
			fe_dcc_update (dcc);

			if (dcc->type == TYPE_SEND || dcc->type == TYPE_RECV)
			{
				if (prefs.dccstalltimeout > 0)
				{
					if (!dcc->throttled
						&& tim - dcc->lasttime > prefs.dccstalltimeout)
					{
						EMIT_SIGNAL (XP_TE_DCCSTALL, dcc->serv->front_session,
										 dcctypes[dcc->type],
										 file_part (dcc->file), dcc->nick, NULL, 0);
						dcc_close (dcc, STAT_ABORTED, FALSE);
					}
				}
			}
			break;
		case STAT_QUEUED:
			if (dcc->type == TYPE_SEND || dcc->type == TYPE_CHATSEND)
			{
				if (tim - dcc->offertime > prefs.dcctimeout)
				{
					if (prefs.dcctimeout > 0)
					{
						EMIT_SIGNAL (XP_TE_DCCTOUT, dcc->serv->front_session,
										 dcctypes[dcc->type],
										 file_part (dcc->file), dcc->nick, NULL, 0);
						dcc_close (dcc, STAT_ABORTED, FALSE);
					}
				}
			}
			break;
		case STAT_DONE:
		case STAT_FAILED:
		case STAT_ABORTED:
			if (prefs.dcc_remove)
				dcc_close (dcc, 0, TRUE);
			break;
		}
		list = next;
	}
}

static int
dcc_connect_sok (struct DCC *dcc)
{
	int sok;
	struct sockaddr_in addr;

	sok = (int)socket (AF_INET, SOCK_STREAM, 0);
	if (sok == -1)
		return -1;

	memset (&addr, 0, sizeof (addr));
	addr.sin_port = htons (dcc->port);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl (dcc->addr);

	set_nonblocking (sok);
	connect (sok, (struct sockaddr *) &addr, sizeof (addr));

	return sok;
}

void
dcc_close (struct DCC *dcc, int dccstat, int destroy)
{
	if (dcc->wiotag)
	{
		net_input_remove (dcc->wiotag);
		dcc->wiotag = 0;
	}

	if (dcc->iotag)
	{
		net_input_remove (dcc->iotag);
		dcc->iotag = 0;
	}

	if (dcc->sok != -1)
	{
		closesocket (dcc->sok);
		dcc->sok = -1;
	}

	dcc_remove_from_sum (dcc);

	if (dcc->fp != -1)
	{
		close (dcc->fp);
		dcc->fp = -1;

		if(dccstat == STAT_DONE)
		{
			/* if we just completed a dcc recieve, move the */
			/* completed file to the completed directory */
			if(dcc->type == TYPE_RECV)
			{			
				/* mgl: change this to use destfile_fs for correctness and to */
				/* handle the case where dccwithnick is set */
				download_move_to_completed_dir(prefs.dccdir, prefs.dcc_completed_dir, 
					dcc->destfile_fs, prefs.dccpermissions);
			}

		}
	}

	dcc->dccstat = dccstat;
	if (dcc->dccchat)
	{
		free (dcc->dccchat);
		dcc->dccchat = NULL;
	}

	if (destroy)
	{
		dcc_list = g_slist_remove (dcc_list, dcc);
		fe_dcc_remove (dcc);
		if (dcc->file)
			free (dcc->file);
		if (dcc->destfile)
			g_free (dcc->destfile);
		if (dcc->destfile_fs)
			g_free (dcc->destfile_fs);
		free (dcc->nick);
		free (dcc);
		return;
	}

	fe_dcc_update (dcc);
}

void
dcc_abort (rage_session *sess, struct DCC *dcc)
{
	if (dcc)
	{
		switch (dcc->dccstat)
		{
		case STAT_QUEUED:
		case STAT_CONNECTING:
		case STAT_ACTIVE:
			dcc_close (dcc, STAT_ABORTED, FALSE);
			switch (dcc->type)
			{
			case TYPE_CHATSEND:
			case TYPE_CHATRECV:
				EMIT_SIGNAL (XP_TE_DCCCHATABORT, sess, dcc->nick, NULL, NULL,
								 NULL, 0);
				break;
			case TYPE_SEND:
				EMIT_SIGNAL (XP_TE_DCCSENDABORT, sess, dcc->nick,
								 file_part (dcc->file), NULL, NULL, 0);
				break;
			case TYPE_RECV:
				EMIT_SIGNAL (XP_TE_DCCRECVABORT, sess, dcc->nick,
								 dcc->file, NULL, NULL, 0);
			}
			break;
		default:
			dcc_close (dcc, 0, TRUE);
		}
	}
}

void
dcc_notify_kill (struct server *serv)
{
	struct server *replaceserv = 0;
	struct DCC *dcc;
	GSList *list = dcc_list;
	if (serv_list)
		replaceserv = (struct server *) serv_list->data;
	while (list)
	{
		dcc = (struct DCC *) list->data;
		if (dcc->serv == serv)
			dcc->serv = replaceserv;
		list = list->next;
	}
}

struct DCC *
dcc_write_chat (char *nick, char *text)
{
	struct DCC *dcc;

	dcc = find_dcc (nick, "", TYPE_CHATRECV);
	if (!dcc)
		dcc = find_dcc (nick, "", TYPE_CHATSEND);
	if (dcc && dcc->dccstat == STAT_ACTIVE)
	{
		char *locale;
		gsize loc_len;
		size_t len;

		len = strlen (text);

		if (dcc->serv->encoding == NULL)	/* system */
		{
			locale = NULL;
			if (!prefs.utf8_locale)
				locale = g_locale_from_utf8 (text, (gssize)len, NULL, &loc_len, NULL);
		} else
		{
			locale = g_convert (text, (gssize)len, dcc->serv->encoding, "UTF-8", 0, &loc_len, 0);
		}

		if (locale)
		{
			text = locale;
			len = loc_len;
		}

		dcc->size += len;
		send (dcc->sok, text, len, 0);
		send (dcc->sok, "\n", 1, 0);
		fe_dcc_update (dcc);
		if (locale)
			g_free (locale);
		return dcc;
	}
	return 0;
}

/* returns: 0 - ok
				1 - the dcc is closed! */

static int
dcc_chat_line (struct DCC *dcc, char *line, char *tbuf)
{
	rage_session *sess;
	char *word[PDIWORDS];
	char *po;
	char *utf;
	char *conv;
	int ret, i;
	size_t len;
	gsize utf_len;

	len = strlen (line);

	if (dcc->serv->encoding == NULL)     /* system */
		utf = g_locale_to_utf8 (line, (int)len, NULL, &utf_len, NULL);
	else
		utf = g_convert (line, (int)len, "UTF-8", dcc->serv->encoding, 0, &utf_len, 0);

	if (utf)
	{
		line = utf;
		len = utf_len;
	}

	/* we really need valid UTF-8 now */
	conv = text_validate (&line, &len);

	sess = find_dialog (dcc->serv, dcc->nick);
	if (!sess)
		sess = dcc->serv->front_session;

	sprintf (tbuf, "%d", dcc->port);

	word[0] = "DCC Chat Text";
	word[1] = net_ip (dcc->addr);
	word[2] = tbuf;
	word[3] = dcc->nick;
	word[4] = line;
	for (i = 5; i < PDIWORDS; i++)
		word[i] = "\000";

	ret = plugin_emit_print (sess, 5, word);

	/* did the plugin close it? */
	if (!g_slist_find (dcc_list, dcc))
	{
		if (utf)
			g_free (utf);
		if (conv)
			g_free (conv);
		return 1;
	}

	/* did the plugin eat the event? */
	if (ret)
	{
		if (utf)
			g_free (utf);
		if (conv)
			g_free (conv);
		return 0;
	}

	url_check (line);

	if (line[0] == 1 && !strncasecmp (line + 1, "ACTION", 6))
	{
		po = strchr (line + 8, '\001');
		if (po)
			po[0] = 0;
		inbound_action (sess, dcc->serv->nick, dcc->nick, line + 8, FALSE);
	} else
	{
		inbound_privmsg (dcc->serv, dcc->nick, "", line, FALSE);
	}
	if (utf)
		g_free (utf);
	if (conv)
		g_free (conv);
	return 0;
}

static gboolean
dcc_read_chat (GIOChannel *source, GIOCondition condition, struct DCC *dcc)
{
	int i, len, dead;
	char tbuf[1226];
	char lbuf[1026];
	char *temp;

	while (1)
	{
		if (dcc->throttled)
		{
			net_input_remove (dcc->iotag);
			dcc->iotag = 0;
			return FALSE;
		}

		if (!dcc->iotag)
			dcc->iotag = net_input_add (dcc->sok, FIA_READ|FIA_EX, dcc_read, dcc);

		len = recv (dcc->sok, lbuf, sizeof (lbuf) - 2, 0);
		if (len < 1)
		{
			if (len < 0)
			{
				if (would_block_again ())
					return TRUE;
			}
			sprintf (tbuf, "%d", dcc->port);
			EMIT_SIGNAL (XP_TE_DCCCHATF, dcc->serv->front_session, dcc->nick,
							 net_ip (dcc->addr), tbuf,
							 errorstring ((len < 0) ? sock_error () : 0), 0);
			dcc_close (dcc, STAT_FAILED, FALSE);
			return TRUE;
		}
		i = 0;
		lbuf[len] = 0;
		while (i < len)
		{
			switch (lbuf[i])
			{
			case '\r':
				break;
			case '\n':
				dcc->dccchat->linebuf[dcc->dccchat->pos] = 0;

				if (prefs.stripcolor)
				{
					temp = strip_color (dcc->dccchat->linebuf);
					dead = dcc_chat_line (dcc, temp, tbuf);
					free (temp);
				} else
				{
					dead = dcc_chat_line (dcc, dcc->dccchat->linebuf, tbuf);
				}

				if (dead || !dcc->dccchat) /* the dcc has been closed, don't use (DCC *)! */
					return TRUE;

				dcc->pos += dcc->dccchat->pos;
				dcc->dccchat->pos = 0;
				fe_dcc_update (dcc);
				break;
			default:
				dcc->dccchat->linebuf[dcc->dccchat->pos] = lbuf[i];
				if (dcc->dccchat->pos < 1022)
					dcc->dccchat->pos++;
			}
			i++;
		}
	}
}

static void
dcc_calc_average_cps (struct DCC *dcc)
{
	time_t sec;

	sec = time (0) - dcc->starttime;
	if (sec < 1)
		sec = 1;
	if (dcc->type == TYPE_SEND)
		dcc->cps = (dcc->ack - dcc->resumable) / (int)sec;
	else
		dcc->cps = (dcc->pos - dcc->resumable) / (int)sec;
}

static void
dcc_send_ack (struct DCC *dcc)
{
	/* send in 64-bit big endian */
	if (dcc->size > G_MAXUINT32)
	{
		guint64 pos = htonll(dcc->pos);
		send (dcc->sok, (char *) &pos, sizeof(pos), 0);
	}
	else
	{
		guint32 pos = g_htonl((guint32)dcc->pos);
		send (dcc->sok, (char *) &pos, sizeof(int), 0);
	}
}

static gboolean
dcc_read (GIOChannel *source, GIOCondition condition, struct DCC *dcc)
{
	char *old;
	char buf[4096];
	int n;
	gboolean need_ack = FALSE;

	if (dcc->fp == -1)
	{

		/* try to create the download dir (even if it exists, no harm) */
		mkdir_utf8 (prefs.dccdir);

		if (dcc->resumable)
		{
			dcc->fp = open (dcc->destfile_fs, O_WRONLY | O_APPEND | OFLAGS);
			dcc->pos = dcc->resumable;
			dcc->ack = dcc->resumable;
		} else
		{
			if (access (dcc->destfile_fs, F_OK) == 0)
			{
				n = 0;
				do
				{
					n++;
					snprintf (buf, sizeof (buf), "%s.%d", dcc->destfile_fs, n);
				}
				while (access (buf, F_OK) == 0);

				g_free (dcc->destfile_fs);
				dcc->destfile_fs = g_strdup (buf);

				old = dcc->destfile;
				dcc->destfile = g_filename_to_utf8 (buf, -1, 0, 0, 0);

				EMIT_SIGNAL (XP_TE_DCCRENAME, dcc->serv->front_session,
								 old, dcc->destfile, NULL, NULL, 0);
				g_free (old);
			}
			dcc->fp =
				open (dcc->destfile_fs, OFLAGS | O_TRUNC | O_WRONLY | O_CREAT,
						prefs.dccpermissions);
		}
	}
	if (dcc->fp == -1)
	{
		/* the last executed function is open(), errno should be valid */
		EMIT_SIGNAL (XP_TE_DCCFILEERR, dcc->serv->front_session, dcc->destfile,
						 errorstring (errno), NULL, NULL, 0);
		dcc_close (dcc, STAT_FAILED, FALSE);
		return TRUE;
	}
	while (1)
	{
		if (dcc->throttled)
		{
			if (need_ack)
				dcc_send_ack (dcc);
			
			net_input_remove (dcc->iotag);
			dcc->iotag = 0;
			return FALSE;
		}

		if (!dcc->iotag)
			dcc->iotag = net_input_add (dcc->sok, FIA_READ|FIA_EX, dcc_read, dcc);

		n = recv (dcc->sok, buf, sizeof (buf), 0);
		if (n < 1)
		{
			if (n < 0)
			{
				if (would_block_again ())
				{
					if (need_ack)
						dcc_send_ack (dcc);
					return TRUE;
				}
			}
			EMIT_SIGNAL (XP_TE_DCCRECVERR, dcc->serv->front_session, dcc->file,
							 dcc->destfile, dcc->nick,
							 errorstring ((n < 0) ? sock_error () : 0), 0);
			/* send ack here? but the socket is dead */ /* FIXME: is this a correct assumption? */
			/* if (need_ack)
			 * 	dcc_send_ack (dcc);*/	
			dcc_close (dcc, STAT_FAILED, FALSE);
			return TRUE;
		}

		if (write (dcc->fp, buf, n) == -1) /* could be out of hdd space */
		{
			EMIT_SIGNAL (XP_TE_DCCRECVERR, dcc->serv->front_session, dcc->file,
							 dcc->destfile, dcc->nick, errorstring (errno), 0);
			if (need_ack)
				dcc_send_ack (dcc);
			dcc_close (dcc, STAT_FAILED, FALSE);
			return TRUE;
		}
		dcc->lasttime = time (0);
		dcc->pos += n;
		need_ack = TRUE;        /* send ack when we're done recv()ing */

		if (dcc->pos >= dcc->size)
		{
			dcc_send_ack (dcc);
			dcc_calc_average_cps (dcc);
			sprintf (buf, "%d", dcc->cps);
			dcc_close (dcc, STAT_DONE, FALSE);
			EMIT_SIGNAL (XP_TE_DCCRECVCOMP, dcc->serv->front_session,
							 dcc->file, dcc->destfile, dcc->nick, buf, 0);
			return TRUE;
		}
	}
}

static gboolean
dcc_connect_finished (GIOChannel *source, GIOCondition condition, struct DCC *dcc)
{
	int er;
	char host[128];
	struct sockaddr_in addr;

	if (dcc->iotag)
	{
		net_input_remove (dcc->iotag);
		dcc->iotag = 0;
	}

#ifdef WIN32
	(void)addr; /* Not used on win32 */

	if (condition & G_IO_ERR)
	{
		int len;

		/* find the last errno for this socket */
		len = sizeof (er);
		getsockopt (dcc->sok, SOL_SOCKET, SO_ERROR, (char *)&er, &len);
		EMIT_SIGNAL (XP_TE_DCCCONFAIL, dcc->serv->front_session,
						 dcctypes[dcc->type], dcc->nick, errorstring (er),
						 NULL, 0);
		dcc->dccstat = STAT_FAILED;
		fe_dcc_update (dcc);
		return TRUE;
	}

#else
	memset (&addr, 0, sizeof (addr));
	addr.sin_port = htons (dcc->port);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl (dcc->addr);

	/* check if it's already connected; This always fails on winXP */
	if (connect (dcc->sok, (struct sockaddr *) &addr, sizeof (addr)) != 0)
	{
		er = sock_error ();
#ifndef WIN32
		if (er != EISCONN)
#else
		if (er != WSAEISCONN)
#endif
		{
			EMIT_SIGNAL (XP_TE_DCCCONFAIL, dcc->serv->front_session,
							 dcctypes[dcc->type], dcc->nick, errorstring (er),
							 NULL, 0);
			dcc->dccstat = STAT_FAILED;
			fe_dcc_update (dcc);
			return TRUE;
		}
	}
#endif

	dcc->dccstat = STAT_ACTIVE;
	snprintf (host, sizeof host, "%s:%d", net_ip (dcc->addr), dcc->port);

	switch (dcc->type)
	{
	case TYPE_RECV:
		dcc->iotag = net_input_add (dcc->sok, FIA_READ|FIA_EX, dcc_read, dcc);
		EMIT_SIGNAL (XP_TE_DCCCONRECV, dcc->serv->front_session,
						 dcc->nick, host, dcc->file, NULL, 0);
		break;
	case TYPE_SEND:
		/* passive send */
		dcc->fastsend = prefs.fastdccsend;
		if (dcc->fastsend)
			dcc->wiotag = net_input_add (dcc->sok, FIA_WRITE, dcc_send_data, dcc);
		dcc->iotag = net_input_add (dcc->sok, FIA_READ|FIA_EX, dcc_read_ack, dcc);
		dcc_send_data (NULL, 0, (gpointer)dcc);
		EMIT_SIGNAL (XP_TE_DCCCONSEND, dcc->serv->front_session,
						 dcc->nick, host, dcc->file, NULL, 0);
		break;
	case TYPE_CHATRECV:
		dcc->iotag = net_input_add (dcc->sok, FIA_READ|FIA_EX, dcc_read_chat, dcc);
		dcc->dccchat = malloc (sizeof (struct dcc_chat));
		dcc->dccchat->pos = 0;
		EMIT_SIGNAL (XP_TE_DCCCONCHAT, dcc->serv->front_session,
						 dcc->nick, host, NULL, NULL, 0);
		break;
	}
	dcc->starttime = time (0);
	dcc->lasttime = dcc->starttime;
	fe_dcc_update (dcc);

	return TRUE;
}

static int dcc_listen_init (struct DCC *, rage_session *);

static void
dcc_connect (struct DCC *dcc)
{
	int ret;
	char tbuf[400];

	if (dcc->dccstat == STAT_CONNECTING)
		return;
	dcc->dccstat = STAT_CONNECTING;

	if (dcc->pasvid && dcc->port == 0)
	{
		/* accepted a passive dcc send */
		ret = dcc_listen_init (dcc, dcc->serv->front_session);
		if (!ret)
		{
			dcc_close (dcc, STAT_FAILED, FALSE);
			return;
		}
		/* possible problems with filenames containing spaces? */
		if (dcc->type == TYPE_RECV)
			snprintf (tbuf, sizeof (tbuf), strchr (dcc->file, ' ') ?
					"DCC SEND \"%s\" %"PRIu64" %d %"PRIu64" %d" :
					"DCC SEND %s %"PRIu64" %d %"PRIu64" %d", 
					dcc->file, (guint64)dcc->addr, 
					dcc->port, (guint64)dcc->size,
					dcc->pasvid);
		else
			snprintf (tbuf, sizeof (tbuf), "DCC CHAT chat %lu %d %d",
				dcc->addr, dcc->port, dcc->pasvid);
		dcc->serv->p_ctcp (dcc->serv, dcc->nick, tbuf);
	}
	else
	{
		dcc->sok = dcc_connect_sok (dcc);
		if (dcc->sok == -1)
		{
			dcc->dccstat = STAT_FAILED;
			fe_dcc_update (dcc);
			return;
		}
		dcc->iotag = net_input_add (dcc->sok, FIA_WRITE|FIA_EX, dcc_connect_finished, dcc);
	}
	
	fe_dcc_update (dcc);
}

static gboolean
dcc_send_data (GIOChannel *source, GIOCondition condition, struct DCC *dcc)
{
	char *buf;
	int len, sent, sok = dcc->sok;

	if (prefs.dcc_blocksize < 1) /* this is too little! */
		prefs.dcc_blocksize = 1024;

	if (prefs.dcc_blocksize > 102400)	/* this is too much! */
		prefs.dcc_blocksize = 102400;

	if (dcc->throttled)
	{
		net_input_remove (dcc->wiotag);
		dcc->wiotag = 0;
		return FALSE;
	}

	if (!dcc->fastsend)
	{
		if (dcc->ack < dcc->pos)
			return TRUE;
	}
	else if (!dcc->wiotag)
		dcc->wiotag = net_input_add (sok, FIA_WRITE, dcc_send_data, dcc);

	buf = malloc (prefs.dcc_blocksize);
	if (!buf)
		return TRUE;

	lseek (dcc->fp, dcc->pos, SEEK_SET);
	len = read (dcc->fp, buf, prefs.dcc_blocksize);
	if (len < 1)
		goto abortit;
	sent = send (sok, buf, len, 0);

	if (sent < 0 && !(would_block ()))
	{
abortit:
		free (buf);
		EMIT_SIGNAL (XP_TE_DCCSENDFAIL, dcc->serv->front_session,
						 file_part (dcc->file), dcc->nick,
						 errorstring (sock_error ()), NULL, 0);
		dcc_close (dcc, STAT_FAILED, FALSE);
		return TRUE;
	}
	if (sent > 0)
	{
		dcc->pos += sent;
		dcc->lasttime = time (0);
	}

	/* have we sent it all yet? */
	if (dcc->pos >= dcc->size)
	{
		/* it's all sent now, so remove the WRITE/SEND handler */
		if (dcc->wiotag)
		{
			net_input_remove (dcc->wiotag);
			dcc->wiotag = 0;
		}
	}

	free (buf);

	return TRUE;
}

static gboolean
dcc_read_ack (GIOChannel *source, GIOCondition condition, struct DCC *dcc)
{
	int len;
	unsigned int ack;
	char buf[16];
	int sok = dcc->sok;

	len = recv (sok, (char *) &ack, 4, MSG_PEEK);
	if (len < 1)
	{
		if (len < 0)
		{
			if (would_block_again ())
				return TRUE;
		}
		EMIT_SIGNAL (XP_TE_DCCSENDFAIL, dcc->serv->front_session,
						 file_part (dcc->file), dcc->nick,
						 errorstring ((len < 0) ? sock_error () : 0), NULL, 0);
		dcc_close (dcc, STAT_FAILED, FALSE);
		return TRUE;
	}
	if (len < 4)
		return TRUE;
	/* if file is larger than 4 gig, assume that the ack is 64 bit */
	if(dcc->size > G_MAXUINT32)
	{
		guint64 ack;
		recv (sok, (char *) &ack, sizeof(ack), 0);
		dcc->ack = ntohll (ack);
	}
	else
	{
		guint32 ack;
		recv (sok, (char *) &ack, sizeof(ack), 0);
		dcc->ack = ntohl (ack);
	}

	/* fix for BitchX */
	if (dcc->ack < dcc->resumable)
		dcc->ackoffset = TRUE;
	if (dcc->ackoffset)
		dcc->ack += dcc->resumable;

	/* DCC complete check */
	if (dcc->pos >= dcc->size && dcc->ack >= dcc->size)
	{
		dcc_calc_average_cps (dcc);
		dcc_close (dcc, STAT_DONE, FALSE);
		sprintf (buf, "%d", dcc->cps);
		EMIT_SIGNAL (XP_TE_DCCSENDCOMP, dcc->serv->front_session,
				file_part (dcc->file), dcc->nick, buf, NULL, 0);
	}
	else if ((!dcc->fastsend) && (dcc->ack >= dcc->pos))
		dcc_send_data (NULL, 0, (gpointer)dcc);

	return TRUE;
}

static gboolean
dcc_accept (GIOChannel *source, GIOCondition condition, struct DCC *dcc)
{
	char host[128];
	struct sockaddr_in CAddr;
	int sok;
	socklen_t len;

	len = sizeof (CAddr);
	sok = (int)accept (dcc->sok, (struct sockaddr *) &CAddr, &len);
	net_input_remove (dcc->iotag);
	dcc->iotag = 0;
	closesocket (dcc->sok);
	if (sok < 0)
	{
		dcc->sok = -1;
		dcc_close (dcc, STAT_FAILED, FALSE);
		return TRUE;
	}
	set_nonblocking (sok);
	dcc->sok = sok;
	dcc->addr = ntohl (CAddr.sin_addr.s_addr);

	if (dcc->pasvid)
		return dcc_connect_finished (NULL, 0, dcc);

	dcc->dccstat = STAT_ACTIVE;
	dcc->lasttime = dcc->starttime = time (0);
	dcc->fastsend = prefs.fastdccsend;

	snprintf (host, sizeof (host), "%s:%d", net_ip (dcc->addr), dcc->port);

	switch (dcc->type)
	{
	case TYPE_SEND:
		if (dcc->fastsend)
			dcc->wiotag = net_input_add (sok, FIA_WRITE, dcc_send_data, dcc);
		dcc->iotag = net_input_add (sok, FIA_READ|FIA_EX, dcc_read_ack, dcc);
		dcc_send_data (NULL, 0, (gpointer)dcc);
		EMIT_SIGNAL (XP_TE_DCCCONSEND, dcc->serv->front_session,
						 dcc->nick, host, dcc->file, NULL, 0);
		break;

	case TYPE_CHATSEND:
		if (prefs.autodialog)
		{
			char *cmd = malloc (8 + strlen (dcc->nick));
			sprintf (cmd, "query %s", dcc->nick);
			handle_command (dcc->serv->server_session, cmd, FALSE);
			free (cmd);
		}
		dcc->iotag = net_input_add (dcc->sok, FIA_READ|FIA_EX, dcc_read_chat, dcc);
		dcc->dccchat = malloc (sizeof (struct dcc_chat));
		dcc->dccchat->pos = 0;
		EMIT_SIGNAL (XP_TE_DCCCONCHAT, dcc->serv->front_session,
						 dcc->nick, host, NULL, NULL, 0);
		break;
	}

	fe_dcc_update (dcc);

	return TRUE;
}

static int
dcc_listen_init (struct DCC *dcc, rage_session *sess)
{
	unsigned long my_addr;
	struct sockaddr_in SAddr;
	struct hostent *dns_query;
	int i, bindretval = -1;
	socklen_t len;

	dcc->sok = (int)socket (AF_INET, SOCK_STREAM, 0);
	if (dcc->sok == -1)
		return FALSE;

	memset (&SAddr, 0, sizeof (struct sockaddr_in));

	len = sizeof (SAddr);
	getsockname (dcc->serv->sok, (struct sockaddr *) &SAddr, &len);

	SAddr.sin_family = AF_INET;

	/*if local_ip is specified use that*/
	if (prefs.local_ip != 0)
	{
		my_addr = prefs.local_ip;
		SAddr.sin_addr.s_addr = prefs.local_ip;
	}
	/*otherwise use the default*/
	else
		my_addr = SAddr.sin_addr.s_addr;

	/*if we have a valid portrange try to use that*/
	if (prefs.first_dcc_send_port > 0)
	{
		SAddr.sin_port = 0;
		i = 0;
		while ((prefs.last_dcc_send_port > ntohs(SAddr.sin_port)) &&
				(bindretval == -1))
		{
			SAddr.sin_port = htons (prefs.first_dcc_send_port + i);
			i++;
			/*printf("Trying to bind against port: %d\n",ntohs(SAddr.sin_port));*/
			bindretval = bind (dcc->sok, (struct sockaddr *) &SAddr, sizeof (SAddr));
		}

		/* with a small port range, reUseAddr is needed */
		len = 1;
		setsockopt (dcc->sok, SOL_SOCKET, SO_REUSEADDR, (char *) &len, sizeof (len));

	}
	else
	{
		/* try random port */
		SAddr.sin_port = 0;
		bindretval = bind (dcc->sok, (struct sockaddr *) &SAddr, sizeof (SAddr));
	}

	if (bindretval == -1)
	{
		/* failed to bind */
		PrintText (sess, "Failed to bind to any address or port.\n");
		return FALSE;
	}

	len = sizeof (SAddr);
	getsockname (dcc->sok, (struct sockaddr *) &SAddr, &len);

	dcc->port = ntohs (SAddr.sin_port);

	/*if we have a dcc_ip, we use that, so the remote client can connect*/
	/*else we try to take an address from dcc_ip_str*/
	/*if something goes wrong we tell the client to connect to our LAN ip*/

	dcc->addr = 0;

	if (prefs.ip_from_server != 0 && prefs.dcc_ip != 0)
		dcc->addr = prefs.dcc_ip;
	else if (prefs.dcc_ip_str[0])
	{
		dns_query = gethostbyname ((const char *) prefs.dcc_ip_str);

		if (dns_query != NULL && dns_query->h_length == 4 &&
				dns_query->h_addr_list[0] != NULL)
		{
			/*we're offered at least one IPv4 address: we take the first*/
			dcc->addr = (unsigned long) *((guint32*) dns_query->h_addr_list[0]);
		}
	}

	/*if nothing else worked we use the address we bound to*/
	if (dcc->addr == 0)
		dcc->addr = my_addr;

	dcc->addr = ntohl (dcc->addr);

	set_nonblocking (dcc->sok);
	listen (dcc->sok, 1);
	set_blocking (dcc->sok);

	dcc->iotag = net_input_add (dcc->sok, FIA_READ|FIA_EX, dcc_accept, dcc);

	return TRUE;
}

void
dcc_add_send (struct DCC *dcc)
{
	char outbuf[512];
	struct stat st;
	char *file, *file_fs = g_filename_from_utf8 (dcc->file, -1, 0, 0, 0);

	if (stat (file_fs, &st) != -1)
	{
		if (sizeof (st.st_size) > 4 && st.st_size > G_MAXUINT32)
			PrintText (dcc->serv->front_session, "Warning, the file you want to send is larger than 4GB and most clients can't handle this. Make sure that the other user either uses Rage as well or another client with 64bit file access.\n");
		if (*file_part (file_fs) && !S_ISDIR (st.st_mode))
		{
			if (st.st_size > 0)
			{
				dcc->starttime = dcc->offertime = time (0);
				dcc->size = (guint64)st.st_size;
				dcc->fp = open (file_fs, OFLAGS | O_RDONLY);
				if (dcc->fp != -1)
				{
					g_free (file_fs);
					if (dcc->pasvid || dcc_listen_init (dcc, dcc->serv->front_session))
					{
						char havespaces = 0;
						file = dcc->file;
						while (*file)
						{
							if (*file == ' ')
							{
								if (prefs.dcc_send_fillspaces)
						    			*file = '_';
							  	else
							   		havespaces = 1;
							}
							file++;
						}
						if (prefs.autoopendccsendwindow)
						{
							if (fe_dcc_open_send_win (TRUE))	/* already open? add */
								fe_dcc_add (dcc);
						}
						else
							fe_dcc_add (dcc);

						if (dcc->pasvid)
						{
							dcc->pasvid = new_id();
							snprintf (outbuf, sizeof (outbuf), (havespaces) ?
									"DCC SEND \"%s\" %lu %d %"PRIu64" %d" :
									"DCC SEND %s %lu %d %"PRIu64" %d",
									file_part (dcc->file), 199ul,
									0, (guint64)dcc->size, dcc->pasvid);
						}
						else
						{
							snprintf (outbuf, sizeof (outbuf), (havespaces) ?
									"DCC SEND \"%s\" %lu %d %"PRIu64 :
									"DCC SEND %s %lu %d %"PRIu64,
									file_part (dcc->file), dcc->addr,
									dcc->port, (guint64)dcc->size);
						}
						dcc->serv->p_ctcp (dcc->serv, dcc->nick, outbuf);

						EMIT_SIGNAL (XP_TE_DCCOFFER, dcc->serv->front_session, file_part (dcc->file),
								dcc->nick, dcc->file, NULL, 0);
					} 
					else
						dcc_close (dcc, 0, TRUE);
					return;
				}
			}
		}
	}
	PrintTextf (dcc->serv->front_session, _("Cannot access %s\n"), dcc->file);
	g_free (file_fs);
	dcc_close (dcc, 0, TRUE);
}

void
dcc_send (rage_session *sess, char *to, char *file, int maxcps, int passive)
{
	const gchar *entry;
	char buf[512];
	GDir *dir;
	struct DCC *dcc;
	GPatternSpec *pattern;

	/* this is utf8 */
	file = expand_homedir (file);

	/* does this filename contain globbings? */
	if (strchr (file, '*') || strchr (file, '?'))
	{
		char path[256];
		char wild[256];
		char *path_fs;  /* local filesystem encoding */

		safe_strcpy (wild, file_part (file), sizeof (wild));
		path_part (file, path, sizeof (path));
		if (path[0] != '/' || path[1] != '\0')
			path[strlen (path) - 1] = 0;    /* remove trailing slash */

		free (file);

		path_fs = g_filename_from_utf8 (path, -1, 0, 0, 0);
		if (path_fs)
		{
			pattern = g_pattern_spec_new(wild);
			dir = g_dir_open(path_fs, 0, NULL);
			if (dir)
			{
				while ((entry = g_dir_read_name(dir)))
				{
					if (g_pattern_match_string(pattern, entry))
					{
						dcc = new_dcc ();
						if (!dcc)
							return;
						dcc->maxcps = maxcps;
						dcc->serv = sess->server;
						dcc->dccstat = STAT_QUEUED;
						dcc->type = TYPE_SEND;
						dcc->nick = g_strdup(to);
						dcc->pasvid = passive;
						
						snprintf(buf, sizeof(buf), "%s/%s", path_fs, entry);
						dcc->file = g_strdup(buf);
						dcc->destfile = g_strdup(buf); /* original filename for reoffer */
						dcc_add_send(dcc);
						buf[0] = 0;
					}
				}
				g_dir_close(dir);
			}
			g_pattern_spec_free(pattern);
			g_free (path_fs);
		}
		return;
	}
	
	/* common dcc */
	dcc = new_dcc ();
	if (!dcc)
		return;
	dcc->maxcps = maxcps;
	dcc->serv = sess->server;
	dcc->dccstat = STAT_QUEUED;
	dcc->type = TYPE_SEND;
	dcc->nick = g_strdup(to);
	dcc->pasvid = passive;
	dcc->file = file;
	dcc->destfile = g_strdup(file); /* original filename for reoffer */
	
	dcc_add_send(dcc);
}

static struct DCC *
find_dcc_from_id (int id, int type)
{
	struct DCC *dcc;
	GSList *list = dcc_list;
	while (list)
	{
		dcc = (struct DCC *) list->data;
		if (dcc->pasvid == id &&
		dcc->dccstat == STAT_QUEUED && dcc->type == type)
		return dcc;
		list = list->next;
	}
	return 0;
}


static struct DCC *
find_dcc_from_port (int port, int type)
{
	struct DCC *dcc;
	GSList *list = dcc_list;
	while (list)
	{
		dcc = (struct DCC *) list->data;
		if (dcc->port == port &&
			 dcc->dccstat == STAT_QUEUED && dcc->type == type)
			return dcc;
		list = list->next;
	}
	return 0;
}

struct DCC *
find_dcc (char *nick, char *file, int type)
{
	GSList *list = dcc_list;
	struct DCC *dcc;
	while (list)
	{
		dcc = (struct DCC *) list->data;
		if (nick == NULL || !rfc_casecmp (nick, dcc->nick))
		{
			if (type == -1 || dcc->type == type)
			{
				if (!file[0])
					return dcc;
				if (!strcasecmp (file, file_part (dcc->file)))
					return dcc;
				if (!strcasecmp (file, dcc->file))
					return dcc;
			}
		}
		list = list->next;
	}
	return 0;
}

/* called when we receive a NICK change from server */

void
dcc_change_nick (struct server *serv, char *oldnick, char *newnick)
{
	struct DCC *dcc;
	GSList *list = dcc_list;

	while (list)
	{
		dcc = (struct DCC *) list->data;
		if (dcc->serv == serv)
		{
			if (!serv->p_cmp (dcc->nick, oldnick))
			{
				if (dcc->nick)
					free (dcc->nick);
				dcc->nick = strdup (newnick);
			}
		}
		list = list->next;
	}
}

void
dcc_get (struct DCC *dcc)
{
	switch (dcc->dccstat)
	{
	case STAT_QUEUED:
		if (dcc->type != TYPE_CHATSEND)
		{
			if (dcc->type == TYPE_RECV && prefs.autoresume && dcc->resumable)
			{
				dcc_resume (dcc);
			}
			else
			{
				dcc->resumable = 0;
				dcc->pos = 0;
				dcc_connect (dcc);
			}
		}
		break;
	case STAT_DONE:
	case STAT_FAILED:
	case STAT_ABORTED:
		dcc_close (dcc, 0, TRUE);
		break;
	}
}

void
dcc_get_nick (rage_session *sess, char *nick)
{
	struct DCC *dcc;
	GSList *list = dcc_list;
	while (list)
	{
		dcc = (struct DCC *) list->data;
		if (!sess->server->p_cmp (nick, dcc->nick))
		{
			if (dcc->dccstat == STAT_QUEUED && dcc->type == TYPE_RECV)
			{
				dcc->resumable = 0;
				dcc->pos = 0;
				dcc->ack = 0;
				dcc_connect (dcc);
				return;
			}
		}
		list = list->next;
	}
	if (sess)
		EMIT_SIGNAL (XP_TE_DCCIVAL, sess, NULL, NULL, NULL, NULL, 0);
}

static struct DCC *
new_dcc (void)
{
	struct DCC *dcc = malloc (sizeof (struct DCC));
	if (!dcc)
		return 0;
	memset (dcc, 0, sizeof (struct DCC));
	dcc->sok = -1;
	dcc->fp = -1;
	dcc_list = g_slist_prepend (dcc_list, dcc);
	return (dcc);
}

void
dcc_chat (rage_session *sess, char *nick)
{
	char outbuf[512];
	struct DCC *dcc;

	dcc = find_dcc (nick, "", TYPE_CHATSEND);
	if (dcc)
	{
		switch (dcc->dccstat)
		{
		case STAT_ACTIVE:
		case STAT_QUEUED:
		case STAT_CONNECTING:
			EMIT_SIGNAL (XP_TE_DCCCHATREOFFER, sess, nick, NULL, NULL, NULL, 0);
			return;
		case STAT_ABORTED:
		case STAT_FAILED:
			dcc_close (dcc, 0, TRUE);
		}
	}
	dcc = find_dcc (nick, "", TYPE_CHATRECV);
	if (dcc)
	{
		switch (dcc->dccstat)
		{
		case STAT_QUEUED:
			dcc_connect (dcc);
			break;
		case STAT_FAILED:
		case STAT_ABORTED:
			dcc_close (dcc, 0, TRUE);
		}
		return;
	}
	/* offer DCC CHAT */

	dcc = new_dcc ();
	if (!dcc)
		return;
	dcc->starttime = dcc->offertime = time (0);
	dcc->serv = sess->server;
	dcc->dccstat = STAT_QUEUED;
	dcc->type = TYPE_CHATSEND;
	dcc->nick = strdup (nick);
	if (dcc_listen_init (dcc, sess))
	{
		if (prefs.autoopendccchatwindow)
		{
			if (fe_dcc_open_chat_win (TRUE))	/* already open? add only */
				fe_dcc_add (dcc);
		} else
			fe_dcc_add (dcc);
		snprintf (outbuf, sizeof (outbuf), "DCC CHAT chat %lu %d",
						dcc->addr, dcc->port);
		dcc->serv->p_ctcp (dcc->serv, nick, outbuf);
		EMIT_SIGNAL (XP_TE_DCCCHATOFFERING, sess, nick, NULL, NULL, NULL, 0);
	} else
	{
		dcc_close (dcc, 0, TRUE);
	}
}

static void
dcc_malformed (rage_session *sess, char *nick, char *data)
{
	EMIT_SIGNAL (XP_TE_MALFORMED, sess, nick, data, NULL, NULL, 0);
}

static int
is_resumable (struct DCC *dcc)
{
	dcc->resumable = 0;

	/* Check the file size */
	if (access (dcc->destfile_fs, W_OK) == 0)
	{
		struct stat st;

		if (stat (dcc->destfile_fs, &st) != -1)
		{
			if (st.st_size < dcc->size)
			{
				dcc->resumable = (off_t)st.st_size;
				dcc->pos = (off_t)st.st_size;
			}
			else
				dcc->resume_error = 2;
		} else
		{
			dcc->resume_errno = errno;
			dcc->resume_error = 1;
		}
	} else
	{
		dcc->resume_errno = errno;
		dcc->resume_error = 1;
	}

	/* Now verify that this DCC is not already in progress from someone else */

	if (dcc->resumable)
	{
		GSList *list = dcc_list;
		struct DCC *d;
		while (list)
		{
			d = list->data;
			if (d->type == TYPE_RECV && d->dccstat != STAT_ABORTED &&
				 d->dccstat != STAT_DONE && d->dccstat != STAT_FAILED)
			{
				if (d != dcc && strcmp (d->destfile, dcc->destfile) == 0)
				{
					dcc->resumable = 0;
					dcc->pos = 0;
					break;
				}
			}
			list = list->next;
		}
	}

	return dcc->resumable;
}

int
dcc_resume (struct DCC *dcc)
{
	char tbuf[500];

	if (dcc->dccstat == STAT_QUEUED && dcc->resumable)
	{
		/* filename contains spaces? Quote them! */
		snprintf (tbuf, sizeof (tbuf) - 10, strchr (dcc->file, ' ') ?
					  "DCC RESUME \"%s\" %d %"PRIu64 :
					  "DCC RESUME %s %d %"PRIu64,
					  dcc->file, dcc->port, (guint64)dcc->resumable);

		if (dcc->pasvid)
 			sprintf (tbuf + strlen (tbuf), " %d", dcc->pasvid);

		dcc->serv->p_ctcp (dcc->serv, dcc->nick, tbuf);
		return 1;
	}

	return 0;
}

static void
dcc_confirm_send (void *ud)
{
	struct DCC *dcc = (struct DCC *) ud;
	dcc_get (dcc);
}

static void
dcc_deny_send (void *ud)
{
	struct DCC *dcc = (struct DCC *) ud;
	dcc_abort (dcc->serv->front_session, dcc);
}

static void
dcc_confirm_chat (void *ud)
{
	struct DCC *dcc = (struct DCC *) ud;
	dcc_connect (dcc);
}

static void
dcc_deny_chat (void *ud)
{
	struct DCC *dcc = (struct DCC *) ud;
	dcc_abort (dcc->serv->front_session, dcc);
}

static struct DCC *
dcc_add_chat (rage_session *sess, char *nick, int port, unsigned long addr, int pasvid)
{
	struct DCC *dcc;

	dcc = new_dcc ();
	if (dcc)
	{
		dcc->serv = sess->server;
		dcc->type = TYPE_CHATRECV;
		dcc->dccstat = STAT_QUEUED;
		dcc->addr = addr;
		dcc->port = port;
		dcc->pasvid = pasvid;
		dcc->nick = strdup (nick);
		dcc->starttime = time (0);

		EMIT_SIGNAL (XP_TE_DCCCHATOFFER, sess->server->front_session, nick,
				NULL, NULL, NULL, 0);

		if (prefs.autoopendccchatwindow)
		{
			if (fe_dcc_open_chat_win (TRUE))        /* already open? add only */
				fe_dcc_add (dcc);
		} 
		else
			fe_dcc_add (dcc);

		if (prefs.autodccchat == 1)
			dcc_connect (dcc);
		else if (prefs.autodccchat == 2)
		{
			char buff[128];
			snprintf (buff, sizeof (buff), "%s is offering DCC Chat.  Do you want to accept?", nick);
			fe_confirm (buff, dcc_confirm_chat, dcc_deny_chat, dcc);
		}
	}
	return dcc;
}

static struct DCC *
dcc_add_file (rage_session *sess, char *file, off_t size, int port, char *nick, unsigned long addr, int pasvid)
{
	struct DCC *dcc;
	char tbuf[512];
	int len;

	dcc = new_dcc ();
	if (dcc)
	{
		dcc->file = strdup (file);

		dcc->destfile = g_malloc (strlen (prefs.dccdir) + strlen (nick) +
				strlen (file) + 4);

		strcpy (dcc->destfile, prefs.dccdir);
		if (prefs.dccdir[strlen (prefs.dccdir) - 1] != '/')
			strcat (dcc->destfile, "/");
		if (prefs.dccwithnick)
		{
#ifdef WIN32
			char *t = strlen (dcc->destfile) + dcc->destfile;
			strcpy (t, nick);
			while (*t)
			{
				if (*t == '\\' || *t == '|')
					*t = '_';
				t++;
			}
#else
			strcat (dcc->destfile, nick);
#endif
			strcat (dcc->destfile, ".");
		}
		strcat (dcc->destfile, file);

		/* get the local filesystem encoding */
		dcc->destfile_fs = g_filename_from_utf8 (dcc->destfile, -1, 0, 0, 0);

		dcc->resumable = 0;
		dcc->pos = 0;
		dcc->serv = sess->server;
		dcc->type = TYPE_RECV;
		dcc->dccstat = STAT_QUEUED;
		dcc->addr = addr;
		dcc->port = port;
		dcc->pasvid = pasvid;
		dcc->size = size;
		dcc->nick = strdup (nick);
		dcc->maxcps = prefs.dcc_max_get_cps;

		is_resumable (dcc);

		/* autodccsend is really autodccrecv.. right? */
		if (prefs.autodccsend == 1)
			dcc_get (dcc);
		else if (prefs.autodccsend == 2)
		{
			char buff[128];
			snprintf (buff, sizeof (buff), "%s is offering \"%s\" via DCC.  Do you want to accept the transfer?", nick, file);
			fe_confirm (buff, dcc_confirm_send, dcc_deny_send, dcc);
		}
		if (prefs.autoopendccrecvwindow)
		{
			if (fe_dcc_open_recv_win (TRUE))        /* was already open? just add*/
				fe_dcc_add (dcc);
		}
		else
			fe_dcc_add (dcc);
	}
	sprintf (tbuf, "%"PRIu64, (guint64)size);
	len = strlen(tbuf) +1;
	snprintf (tbuf + len, 300, "%s:%d", net_ip (dcc->addr), dcc->port);
	EMIT_SIGNAL (XP_TE_DCCSENDOFFER, sess->server->front_session, nick,
			file, tbuf, tbuf + len, 0);
	return dcc;
}

void
handle_dcc (rage_session *sess, char *nick, char *ctcp_data)
{
	char tbuf[512];
	char *parv[MAX_TOKENS], *type;
	int parc;
	struct DCC *dcc;
	int port, pasvid = 0;
	unsigned long addr;
	off_t size;

	strcpy(tbuf, ctcp_data); /* FIXME: ok? */
	split_cmd_parv(tbuf, &parc, parv);

	type = parv[0];
	
	switch(MAKE4(toupper(type[0]),toupper(type[1]),
				toupper(type[2]),toupper(type[3])))
	{
		case D_CHAT:
		{
			port = atoi (parv[3]);
			addr = strtoul (parv[2], NULL, 10);

			if (port == 0)
				pasvid = atoi (parv[4]);

			if (!addr /*|| (port < 1024 && port != 0)*/
				|| port > 0xffff || (port == 0 && pasvid == 0))
			{
				dcc_malformed (sess, nick, ctcp_data);
				return;
			}
			dcc = find_dcc (nick, "", TYPE_CHATSEND);
			if (dcc)
				dcc_close (dcc, 0, TRUE);

			dcc = find_dcc (nick, "", TYPE_CHATRECV);
			if (dcc)
				dcc_close (dcc, 0, TRUE);

			dcc_add_chat (sess, nick, port, addr, pasvid);
			return;
		}
		case D_RESUME:
		{
			port = atoi (parv[2]);

			if (port == 0)
			{ /* PASSIVE */
				pasvid = atoi(parv[4]);
				dcc = find_dcc_from_id(pasvid, TYPE_SEND);
			} else
				dcc = find_dcc_from_port (port, TYPE_SEND);
			
			if (!dcc)
				dcc = find_dcc (nick, parv[1], TYPE_SEND);

			if (dcc)
			{
				size = strtoull (parv[3], NULL, 10);
				dcc->resumable = size;
				if (dcc->resumable < dcc->size)
				{
					dcc->pos = dcc->resumable;
					dcc->ack = dcc->resumable;
					lseek (dcc->fp, dcc->pos, SEEK_SET);

					/* Checking if dcc is passive and if filename contains spaces */
					if (dcc->pasvid)
						snprintf (tbuf, sizeof (tbuf), strchr (file_part (dcc->file), ' ') ?
								"DCC ACCEPT \"%s\" %d %"PRIu64" %d" :
								"DCC ACCEPT %s %d %"PRIu64" %d",
								file_part (dcc->file), port, 
								(guint64)dcc->resumable, dcc->pasvid);
					else
						snprintf (tbuf, sizeof (tbuf), strchr (file_part (dcc->file), ' ') ?
								"DCC ACCEPT \"%s\" %d %"PRIu64 :
								"DCC ACCEPT %s %d %"PRIu64,
								file_part (dcc->file), port, 
								(guint64)dcc->resumable);

					dcc->serv->p_ctcp (dcc->serv, dcc->nick, tbuf);
				}
				sprintf (tbuf, "%"PRIu64, (guint64)dcc->pos);
				EMIT_SIGNAL (XP_TE_DCCRESUMEREQUEST, sess, nick,
								 file_part (dcc->file), tbuf, NULL, 0);
			}
			return;
		}
		case D_ACCEPT:
		{
			port = atoi (parv[2]);
			dcc = find_dcc_from_port (port, TYPE_RECV);
			if (dcc && dcc->dccstat == STAT_QUEUED)
				dcc_connect (dcc);
			return;
		}
		case D_SEND:
		{
			char *file = file_part (parv[1]);
			int psend = 0;

			port = atoi (parv[3]);
			addr = strtoul (parv[2], NULL, 10);
			size = strtoull (parv[4], NULL, 10);

			if (port == 0) /* Passive dcc requested */
				pasvid = atoi (parv[5]);
			else if (parv[5][0] != 0)
			{
				/* Requesting passive dcc.
				 * Destination user of an active dcc is giving his
				 * TRUE address/port/pasvid data.
				 * This information will be used later to
				 * establish the connection to the user.
				 * We can recognize this type of dcc using parv[5]
				 * because this field is always null (no pasvid)
				 * in normal dcc sends.
				 */
				pasvid = atoi (parv[5]);
				psend = 1;
			}


			if (!addr || !size /*|| (port < 1024 && port != 0)*/
				|| port > 0xffff || (port == 0 && pasvid == 0))
			{
				dcc_malformed (sess, nick, ctcp_data);
				return;
			}

			if (psend)
			{
				/* Third Step of Passive send.
				 * Connecting to the destination and finally
				 * sending file.
				 */
				dcc = find_dcc_from_id (pasvid, TYPE_SEND);
				if (dcc)
				{
					dcc->addr = addr;
					dcc->port = port;
					dcc_connect (dcc);
				} 
				else
					dcc_malformed (sess, nick, ctcp_data);
				return;
			}

			dcc_add_file (sess, file, size, port, nick, addr, pasvid);
			return;
		}
		default:
			EMIT_SIGNAL (XP_TE_DCCGENERICOFFER, sess->server->front_session,
							 ctcp_data, nick, NULL, NULL, 0);
			return;
	}
}

void
dcc_show_list (rage_session *sess)
{
	char pos[16], size[16];
	int i = 0;
	struct DCC *dcc;
	GSList *list = dcc_list;

	EMIT_SIGNAL (XP_TE_DCCHEAD, sess, NULL, NULL, NULL, NULL, 0);
	while (list)
	{
		dcc = (struct DCC *) list->data;
		i++;
		capacity_format_size(size, sizeof(size), dcc->size);
		capacity_format_size(pos, sizeof(pos), dcc->pos);
		PrintTextf (sess, " %s  %-10.10s %-7.7s %-7s %-7s %s\n",
					 dcctypes[dcc->type], dcc->nick,
					 _(dccstat[dcc->dccstat].name), 
					 size, pos, file_part (dcc->file));
		list = list->next;
	}
	if (!i)
		PrintText (sess, _("No active DCCs\n"));
}
