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

typedef struct
{
	server *serv;
	char *op;
	char *deop;
	char *voice;
	char *devoice;
} mode_run;

/* word[] - list of nicks.
   wpos   - index into word[]. Where nicks really start.
   end    - index into word[]. Last entry plus one.
   sign   - a char, e.g. '+' or '-'
   mode   - a mode, e.g. 'o' or 'v'	*/
void
send_channel_modes (session *sess, char *word[], int wpos, int end, 
		char sign, char mode)
{
	int usable_modes, i;
	size_t orig_len, len, wlen, max;
	server *serv = sess->server;
	int modes_per_line;
	char tbuf[503];

	/* sanity check. IRC RFC says three per line. */
	if (serv->modes_per_line < 3)
		serv->modes_per_line = 3;

	modes_per_line = serv->modes_per_line;

	/* RFC max, minus length of "MODE %s " and "\r\n" and 1 +/- sign */
	/* 512 - 6 - 2 - 1 - strlen(chan) */
	max = 503 - (int)strlen (sess->channel);

	while (wpos < end)
	{
		tbuf[0] = '\0';
		orig_len = len = 0;

		/* we'll need this many modechars too */
		len += modes_per_line;

		/* how many can we fit? */
		for (i = 0; i < modes_per_line; i++)
		{
			/* no more nicks left? */
			if (wpos + i >= end)
				break;
			wlen = strlen (word[wpos + i]) + 1;
			if (wlen + len > max)
				break;
			len += wlen; /* length of our whole string so far */
		}
		if (i < 1)
			return;
		usable_modes = i;	/* this is how many we'll send on this line */

		/* add the +/-modemodemodemode */
		len = orig_len;
		tbuf[len] = sign;
		len++;
		for (i = 0; i < usable_modes; i++)
		{
			tbuf[len] = mode;
			len++;
		}
		tbuf[len] = 0;	/* null terminate for the strcat() to work */

		/* add all the nicknames */
		for (i = 0; i < usable_modes; i++)
		{
			strcat (tbuf, " ");
			strcat (tbuf, word[wpos + i]);
		}
		serv->p_mode (serv, sess->channel, tbuf);

		wpos += usable_modes;
	}
}

/* does 'chan' have a valid prefix? e.g. # or & */

int
is_channel (server * serv, char *chan)
{
	char *ct = get_isupport(serv, "CHANTYPES");
	if (!ct)
		return 0;
	if (strchr(ct, chan[0]))
		return 1;
	return 0;
}

/* is the given char a valid nick mode char? e.g. @ or + */

static int
is_prefix_char (server * serv, char c)
{
	int pos = 0;
	char *np = serv->nick_prefixes;

	while (np[0])
	{
		if (np[0] == c)
			return pos;
		pos++;
		np++;
	}

	if (serv->bad_prefix)
	{
		if (strchr (serv->bad_nick_prefixes, c))
		/* valid prefix char, but mode unknown */
			return -2;
	}

	return -1;
}

/* returns '@' for ops etc... */

char
get_nick_prefix (server * serv, unsigned int access)
{
	int pos;
	char c;

	for (pos = 0; pos < USERACCESS_SIZE; pos++)
	{
		c = serv->nick_prefixes[pos];
		if (c == 0)
			break;
		if (access & (1 << pos))
			return c;
	}

	return 0;
}

/* returns the access bitfield for a nickname. E.g.
	@nick would return 000010 in binary
	%nick would return 000100 in binary
	+nick would return 001000 in binary */

unsigned int
nick_access (server * serv, char *nick, int *modechars)
{
	int i;
	unsigned int access = 0;
	char *orig = nick;

	while (*nick)
	{
		i = is_prefix_char (serv, *nick);
		if (i == -1)
			break;

		/* -2 == valid prefix char, but mode unknown */
		if (i != -2)
			access |= (1 << i);

		nick++;
	}

	*modechars = (int)(nick - orig);

	return access;
}

/* returns the access number for a particular mode. e.g.
	mode 'a' returns 0
	mode 'o' returns 1
	mode 'h' returns 2
	mode 'v' returns 3
	Also puts the nick-prefix-char in 'prefix' */

int
mode_access (server * serv, char mode, char *prefix)
{
	int pos = 0;

	while (serv->nick_modes[pos])
	{
		if (serv->nick_modes[pos] == mode)
		{
			*prefix = serv->nick_prefixes[pos];
			return pos;
		}
		pos++;
	}

	*prefix = 0;

	return -1;
}

static int
mode_timeout_cb (session *sess)
{
	if (is_session (sess))
	{
		sess->mode_timeout_tag = 0;
		sess->server->p_join_info (sess->server, sess->channel);
		sess->ignore_mode = TRUE;
		sess->ignore_date = TRUE;
	}
	return 0;
}

static void
record_chan_mode (session *sess)/*, char sign, char mode, char *arg)*/
{
	/* Should we write a routine to add sign,mode to sess->current_modes?
		nah! too hard. Lets just issue a MODE #channel and read it back.
		We need to find out the new modes for the titlebar, but let's not
		flood ourselves off when someone decides to change 100 modes/min. */
	if (!sess->mode_timeout_tag)
		sess->mode_timeout_tag = fe_timeout_add (15000, mode_timeout_cb, sess);
}

static char *
mode_cat (char *str, char *addition)
{
	size_t len;

	if (str)
	{
		len = strlen (str) + strlen (addition) + 2;
		str = realloc (str, len);
		strcat (str, " ");
		strcat (str, addition);
	} else
	{
		str = strdup (addition);
	}

	return str;
}

/* handle one mode, e.g.
   handle_single_mode (mr,'+','b',"elite","#warez","banneduser",) */

static void
handle_single_mode (mode_run *mr, char sign, char mode, char *nick,
						  char *chan, char *arg, int quiet, int is_324)
{
	session *sess;
	server *serv = mr->serv;
	char outbuf[4];

	outbuf[0] = sign;
	outbuf[1] = 0;
	outbuf[2] = mode;
	outbuf[3] = 0;

	sess = find_channel (serv, chan);
	if (!sess || !is_channel (serv, chan))
	{
		/* got modes for a chan we're not in! probably nickmode +isw etc */
		sess = serv->front_session;
		goto genmode;
	}

	/* is this a nick mode? */
	if (strchr (serv->nick_modes, mode))
	{
		/* update the user in the userlist */
		ul_update_entry (sess, /*nickname */ arg, mode, sign);
	} else
	{
		if (!is_324 && !sess->ignore_mode)
			record_chan_mode (sess);/*, sign, mode, arg);*/
	}

	switch (sign)
	{
	case '+':
		switch (mode)
		{
		case 'k':
			safe_strcpy (sess->channelkey, arg, sizeof (sess->channelkey));
			fe_update_channel_key (sess);
			fe_update_mode_buttons (sess, mode, sign);
			if (!quiet)
				EMIT_SIGNAL (XP_TE_CHANSETKEY, sess, nick, arg, NULL, NULL, 0);
			return;
		case 'l':
			sess->limit = atoi (arg);
			fe_update_channel_limit (sess);
			fe_update_mode_buttons (sess, mode, sign);
			if (!quiet)
				EMIT_SIGNAL (XP_TE_CHANSETLIMIT, sess, nick, arg, NULL, NULL, 0);
			return;
		case 'o':
			if (!quiet)
				mr->op = mode_cat (mr->op, arg);
			return;
		case 'h':
			if (!quiet)
				EMIT_SIGNAL (XP_TE_CHANHOP, sess, nick, arg, NULL, NULL, 0);
			return;
		case 'v':
			if (!quiet)
				mr->voice = mode_cat (mr->voice, arg);
			return;
		case 'b':
			if (!quiet)
				EMIT_SIGNAL (XP_TE_CHANBAN, sess, nick, arg, NULL, NULL, 0);
			return;
		case 'e':
			if (!quiet)
				EMIT_SIGNAL (XP_TE_CHANEXEMPT, sess, nick, arg, NULL, NULL, 0);
			return;
		case 'I':
			if (!quiet)
				EMIT_SIGNAL (XP_TE_CHANINVITE, sess, nick, arg, NULL, NULL, 0);
			return;
		}
		break;
	case '-':
		switch (mode)
		{
		case 'k':
			sess->channelkey[0] = 0;
			fe_update_channel_key (sess);
			fe_update_mode_buttons (sess, mode, sign);
			if (!quiet)
				EMIT_SIGNAL (XP_TE_CHANRMKEY, sess, nick, NULL, NULL, NULL, 0);
			return;
		case 'l':
			sess->limit = 0;
			fe_update_channel_limit (sess);
			fe_update_mode_buttons (sess, mode, sign);
			if (!quiet)
				EMIT_SIGNAL (XP_TE_CHANRMLIMIT, sess, nick, NULL, NULL, NULL, 0);
			return;
		case 'o':
			if (!quiet)
				mr->deop = mode_cat (mr->deop, arg);
			return;
		case 'h':
			if (!quiet)
				EMIT_SIGNAL (XP_TE_CHANDEHOP, sess, nick, arg, NULL, NULL, 0);
			return;
		case 'v':
			if (!quiet)
				mr->devoice = mode_cat (mr->devoice, arg);
			return;
		case 'b':
			if (!quiet)
				EMIT_SIGNAL (XP_TE_CHANUNBAN, sess, nick, arg, NULL, NULL, 0);
			return;
		case 'e':
			if (!quiet)
				EMIT_SIGNAL (XP_TE_CHANRMEXEMPT, sess, nick, arg, NULL, NULL, 0);
			return;
		case 'I':
			if (!quiet)
				EMIT_SIGNAL (XP_TE_CHANRMINVITE, sess, nick, arg, NULL, NULL, 0);
			return;
		}
	}

	fe_update_mode_buttons (sess, mode, sign);

 genmode:
	if (!quiet)
	{
		if (*arg)
		{
			char *buf = malloc (strlen (chan) + strlen (arg) + 2);
			sprintf (buf, "%s %s", chan, arg);
			EMIT_SIGNAL (XP_TE_CHANMODEGEN, sess, nick, outbuf, outbuf + 2, buf, 0);
			free (buf);
		} else
			EMIT_SIGNAL (XP_TE_CHANMODEGEN, sess, nick, outbuf, outbuf + 2, chan, 0);
	}
}

/* does this mode have an arg? like +b +l +o */

static int
mode_has_arg (server * serv, char sign, char mode)
{
	char *cm;
	int type;

	/* if it's a nickmode, it must have an arg */
	if (strchr (serv->nick_modes, mode))
		return 1;

	/* see what numeric 005 CHANMODES=xxx said */
	cm = get_isupport(serv, "CHANMODES");
	type = 0;
	while (*cm)
	{
		if (*cm == ',')
		{
			type++;
		} else if (*cm == mode)
		{
			switch (type)
			{
			case 0:					  /* type A */
			case 1:					  /* type B */
				return 1;
			case 2:					  /* type C */
				if (sign == '+')
					return 1;
			case 3:					  /* type D */
				return 0;
			}
		}
		cm++;
	}

	return 0;
}

/* handle a MODE or numeric 324 from server */

void
handle_mode (server * serv, int parc, char *parv[],
				 char *nick, int numeric_324)
{
	session *sess;
	char buf[200]; /* FIXME: Is this large enough? */
	char *chan, *modes, *argstr;
	char sign;
	int arg;
	int num_args;
	int num_modes;
	int all_modes_have_args = FALSE;
	int using_front_tab = FALSE;
	unsigned int i, offset = 2;
	mode_run mr;
	size_t size;

	mr.serv = serv;
	mr.op = mr.deop = mr.voice = mr.devoice = NULL;

	/* numeric 324 has everything 1 word later (as opposed to MODE) */
	if (numeric_324)
		offset++;

	chan = parv[offset];
	modes = parv[offset + 1];

	if (*modes == 0)
		return;	/* beyondirc's blank modes */

	sess = find_channel (serv, chan);
	if (!sess)
	{
		sess = serv->front_session;
		using_front_tab = TRUE;
	}

	if (prefs.raw_modes && !numeric_324)
	{
		buf[0] = 0;
		paste_parv(buf, sizeof(buf), offset, parc, parv);
		EMIT_SIGNAL (XP_TE_RAWMODES, sess, nick, buf, 0, 0, 0);
	}

	if (numeric_324 && !using_front_tab)
	{
		if (sess->current_modes)
			free (sess->current_modes);
		buf[0] = 0;
		paste_parv(buf, sizeof(buf), offset+1, parc, parv);
		sess->current_modes = strdup (buf);
		fe_set_title (sess);
	}

	sign = *modes;
	modes++;
	arg = 1;

	/* count the number of arguments (e.g. after the -o+v) */
	num_args = 0;
	i = 2;
	while (i+offset < (unsigned)parc)
	{
		if (!(*parv[i + offset]))
			break;
		i++;
		num_args++;
	}

	/* count the number of modes (without the -/+ chars */
	num_modes = 0;
	i = 0;
	size = strlen(modes);
	while (i < size)
	{
		if (modes[i] != '+' && modes[i] != '-')
			num_modes++;
		i++;
	}

	if (num_args == num_modes)
		all_modes_have_args = TRUE;

	while (1)
	{
		switch (*modes)
		{
		case 0:
			goto xit;
		case '-':
		case '+':
			sign = *modes;
			break;
		default:
			argstr = "";
			if (all_modes_have_args || mode_has_arg (serv, sign, *modes))
			{
				arg++;
				argstr = parv[arg + offset];
			}
			handle_single_mode (&mr, sign, *modes, nick, chan,
									  argstr, numeric_324 || prefs.raw_modes,
									  numeric_324);
		}

		modes++;
	}

xit:
	/* print all the grouped Op/Deops */
	if (mr.op)
	{
		EMIT_SIGNAL (XP_TE_CHANOP, sess, nick, mr.op, NULL, NULL, 0);
		free (mr.op);
	}

	if (mr.deop)
	{
		EMIT_SIGNAL (XP_TE_CHANDEOP, sess, nick, mr.deop, NULL, NULL, 0);
		free (mr.deop);
	}

	if (mr.voice)
	{
		EMIT_SIGNAL (XP_TE_CHANVOICE, sess, nick, mr.voice, NULL, NULL, 0);
		free (mr.voice);
	}

	if (mr.devoice)
	{
		EMIT_SIGNAL (XP_TE_CHANDEVOICE, sess, nick, mr.devoice, NULL, NULL, 0);
		free (mr.devoice);
	}
}

/* support commands for 005 stuff */
char *
get_isupport(server * serv, char *value)
{
	int found;
	char *data;

	data = dict_find(serv->isupport, value, &found);
	return found ? data : NULL;
}

int
isupport(server * serv, char *value)
{
	int found;
	
	dict_find(serv->isupport, value, &found);
	return found;
}

/* handle the 005 numeric */


/* hack added here by bart: 04/09/2004 */
#ifdef WIN32
int strcasecmp(const char *s1, const char *s2)
{
	return strcmpi(s1, s2);
}
#endif

void
inbound_005 (server * serv, int parc, char *parv[])
{
	int w;
	char *pre;

	w = 3;
	parc--; 
	while (w < parc && *parv[w])
	{
		pre = strchr(parv[w], '=');
		if (pre)
			pre[0] = 0;
		dict_insert(serv->isupport, g_strdup(parv[w]), pre ? g_strdup(pre +1) : NULL);
		if (pre)
			pre[0] = '=';
		w++;
	}
}

void
run_005 (server * serv)
{
	char *pre;

	if((pre = get_isupport(serv, "CASEMAPPING")))
	{
		if (strcmp (pre, "ascii") == 0)
			serv->p_cmp = (void *)strcasecmp;
	}
	
	
	if((pre = get_isupport(serv, "CHARSET")))
	{
		if (strcasecmp (pre, "UTF-8") == 0)
		{
			if (serv->encoding)
				free (serv->encoding);
			serv->encoding = strdup(pre);
		}
	}
	
	if((pre = get_isupport(serv, "MODES")))
		serv->modes_per_line = atoi (pre);
	
	if((pre = get_isupport(serv, "NAMESX")))
		tcp_send_len (serv, "PROTOCTL NAMESX\r\n", 17);
	
	if((pre = get_isupport(serv, "NETWORK")))
	{
		if (serv->server_session->type == SESS_SERVER)
		{
			safe_strcpy (serv->server_session->channel, pre, CHANLEN);
			fe_set_channel (serv->server_session);
		}
	}
	
	if((pre = get_isupport(serv, "PREFIX")))
	{
		char *new;
		
		new = strchr(pre, ')');
		if (new)
		{
			new[0] = 0; /* NULL out the ')' */
			free (serv->nick_prefixes);
			free (serv->nick_modes);
			serv->nick_prefixes = strdup (new + 1);
			serv->nick_modes = strdup (pre + 1);
			
		} else
		{
			/* bad! some ircds don't give us the modes. */
			/* in this case, we use it only to strip /NAMES */
			serv->bad_prefix = TRUE;
			if (serv->bad_nick_prefixes)
				free (serv->bad_nick_prefixes);
			serv->bad_nick_prefixes = strdup (pre);
		}
	}

	if(isupport(serv, "CAPAB")) /* after this we get a 290 numeric reply */
		 tcp_send_len (serv, "CAPAB IDENTIFY-MSG\r\n", 20);

}
