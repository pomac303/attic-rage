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

#include "fe-gtk.h"
#include <glade/glade.h>

GdkPixmap *channelwin_pix;


#ifdef USE_XLIB

static void
redraw_trans_xtexts (void)
{
	GSList *list = sess_list;
	rage_session *sess;
	int done_main = FALSE;

	while (list)
	{
		sess = list->data;
#ifdef USE_ZVT
		if (sess->type == SESS_SHELL)
		{
			menu_newshell_set_palette (sess);
			gtk_widget_queue_draw (sess->gui->xtext);
		} else
#endif
		{
			if (GTK_XTEXT (sess->gui->xtext)->transparent)
			{
				if (!sess->gui->is_tab || !done_main)
					gtk_xtext_refresh (GTK_XTEXT (sess->gui->xtext), 1);
				if (sess->gui->is_tab)
					done_main = TRUE;
			}
		}
		list = list->next;
	}
}

static GdkFilterReturn
root_event_cb (GdkXEvent *xev, GdkEventProperty *event, gpointer data)
{
	Atom at = None;
	XEvent *xevent = (XEvent *)xev;

	if (xevent->type == PropertyNotify)
	{
		if (at == None)
			at = XInternAtom (xevent->xproperty.display, "_XROOTPMAP_ID", True);

		if (at == xevent->xproperty.atom)
			redraw_trans_xtexts ();
	}

	return GDK_FILTER_CONTINUE;
}

#endif

int
fe_args (int argc, char *argv[])
{
	int offset = 0;

#ifdef ENABLE_NLS
	bindtextdomain (PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);
#endif

	if (argc > 1)
	{
		if (!strcasecmp (argv[1], "-v") || !strcasecmp (argv[1], "--version"))
		{
			printf (PACKAGE" "VERSION"-%s\n", rage_svn_version);
			return 0;
		}
		if (!strcasecmp (argv[1], "-p"))
		{
#ifdef WIN32
			/* see the chdir() below */
			char *sl, *exe = strdup (argv[0]);
			sl = strrchr (exe, '\\');
			if (sl)
			{
				*sl = 0;
				printf ("%s\\plugins\n", exe);
			}
#else
			printf ("%s\n", RAGELIBDIR"/plugins");
#endif
			return 0;
		}
		if (!strcasecmp (argv[1], "-u"))
		{
			printf ("%s\n", get_xdir_fs ());
			return 0;
		}
		if (!strcasecmp (argv[1], "-h") || !strcasecmp (argv[1], "--help"))
		{
			printf (PACKAGE" "VERSION"-%s\n"
					"Usage: %s [OPTIONS]... [URL]\n\n", rage_svn_version,
					argv[0]);
			printf ("%s:\n"
					"  -a,  --no-auto            %s\n"
					"  -d,  --cfgdir %-11s %s\n"
					"  -n,  --no-plugins         %s\n"
					"  -p                        %s\n"
					"  -u                        %s\n"
					"  -v,  --version            %s\n\n"
					"URL:\n"
					"  irc://server:port/channel\n\n",
						_("Options"),
						_("don't auto connect"),
						_("DIRECTORY"),
						_("use a different config dir"),
						_("don't auto load any plugins"),
						_("show plugin auto-load dir"),
						_("show user config dir"),
						_("show version information")
					);
			return 0;
		}
		if (!strcasecmp (argv[1], "-a") || !strcasecmp (argv[1], "--no-auto"))
		{
			auto_connect = 0;
			offset++;
		}
		if (argc > 1 + offset)
		{
			if (!strcasecmp (argv[1+offset], "-n") || !strcasecmp (argv[1+offset], "--no-plugins"))
			{
				skip_plugins = 1;
				offset++;
			}
		}
	}

#ifdef WIN32
	/* this is mainly for irc:// URL handling. When windows calls us from */
	/* I.E, it doesn't give an option of "Start in" directory, like short */
	/* cuts can. So we have to set the current dir manually, to the path  */
	/* of the exe. */
	{
		char *tmp = strdup (argv[0]);
		char *sl;

		sl = strrchr (tmp, '\\');
		if (sl)
		{
			*sl = 0;
			chdir (tmp);
		}
		free (tmp);
	}
#endif

	if (argc > 2 + offset)
	{
		if (!strcasecmp (argv[1+offset], "-d") || !strcasecmp (argv[1+offset], "--cfgdir"))
		{
			xdir_fs = strdup (argv[2+offset]);
			if (xdir_fs[strlen (xdir_fs) - 1] == '/')
				xdir_fs[strlen (xdir_fs) - 1] = 0;
			offset += 2;
		}
	}

	if (argc > (offset + 1))
		connect_url = strdup (argv[offset + 1]);

	gtk_init (&argc, &argv);

#ifdef USE_XLIB
/*	XSelectInput (GDK_DISPLAY (), GDK_ROOT_WINDOW (), PropertyChangeMask);*/
	gdk_window_set_events (gdk_get_default_root_window (), GDK_PROPERTY_CHANGE_MASK);
	gdk_window_add_filter (gdk_get_default_root_window (),
								  (GdkFilterFunc)root_event_cb, NULL);
#endif

	return 1;
}

const char cursor_color_rc[] =
	"style \"xc-ib-st\""
	"{"
		"GtkEntry::cursor-color=\"#%02x%02x%02x\""
	"}"
	"widget \"*.xchat-inputbox\" style : application \"xc-ib-st\"";

GtkStyle *
create_input_style (void)
{
	GtkStyle *style;
	char buf[256];
	static int done_rc = FALSE;

	style = gtk_style_new ();
	pango_font_description_free (style->font_desc);
	style->font_desc = pango_font_description_from_string (prefs.font_normal);

	/* fall back */
	if (pango_font_description_get_size (style->font_desc) == 0)
	{
		snprintf (buf, sizeof (buf), _("Failed to open font:\n\n%s"), prefs.font_normal);
		gtkutil_simpledialog (buf);
		pango_font_description_free (style->font_desc);
		style->font_desc = pango_font_description_from_string ("sans 11");
	}

	if (prefs.style_inputbox && !done_rc)
	{
		done_rc = TRUE;
		sprintf (buf, cursor_color_rc, (colors[COL_FG].red >> 8),
			(colors[COL_FG].green >> 8), (colors[COL_FG].blue >> 8));
		gtk_rc_parse_string (buf);
	}

	style->bg[GTK_STATE_NORMAL] = colors[COL_FG];
	style->base[GTK_STATE_NORMAL] = colors[COL_BG];
	style->text[GTK_STATE_NORMAL] = colors[COL_FG];

	return style;
}

void
fe_init (void)
{
	palette_load ();
	key_init ();
	pixmaps_init ();

	channelwin_pix = pixmap_load_from_file (prefs.background);
	input_style = create_input_style ();
}

void
fe_main (void)
{
  // Breaking in
  GladeXML *xml = glade_xml_new("./src/ui-gtk/ui-gtk.glade", NULL, NULL);  
  glade_xml_signal_autoconnect(xml);

	gtk_main ();
	
	/* sleep for 3 seconds so any QUIT messages are not lost. The  */
	/* GUI is closed at this point, so the user doesn't even know! */
	if (prefs.wait_on_exit)
		sleep (3);
}

void
fe_cleanup (void)
{
	/* it's saved when pressing OK in setup.c */
	/*palette_save ();*/
}

void
fe_exit (void)
{
	gtk_main_quit ();
}

#ifdef WIN32

static void
log_handler (const gchar   *log_domain,
		       GLogLevelFlags log_level,
		       const gchar   *message,
		       gpointer	      unused_data)
{
	rage_session *sess;

	if (getenv ("XCHAT_WARNING_IGNORE"))
		return;

	sess = find_dialog (serv_list->data, "(warnings)");
	if (!sess)
		sess = new_ircwindow (serv_list->data, "(warnings)", SESS_DIALOG, 0);

	PrintTextf (sess, "%s\t%s\n", log_domain, message);
	if (getenv ("XCHAT_WARNING_ABORT"))
		abort ();
}

#endif

void
fe_new_window (rage_session *sess, int focus)
{
	int tab = FALSE;

	if (sess->type == SESS_DIALOG)
	{
		if (prefs.privmsgtab)
			tab = TRUE;
	} 
	else
	{
		if (prefs.tabchannels)
			tab = TRUE;
	}

	mg_changui_new (sess, NULL, tab, focus);

#ifdef WIN32
	g_log_set_handler ("GLib", G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING, (GLogFunc)log_handler, 0);
	g_log_set_handler ("GLib-GObject", G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING, (GLogFunc)log_handler, 0);
	g_log_set_handler ("Gdk", G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING, (GLogFunc)log_handler, 0);
	g_log_set_handler ("Gtk", G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING, (GLogFunc)log_handler, 0);
#endif
}

void
fe_new_server (struct server *serv)
{
	serv->gui = malloc (sizeof (struct server_gui));
	memset (serv->gui, 0, sizeof (struct server_gui));
}

static void
null_this_var (GtkWidget * unused, GtkWidget ** dialog)
{
	*dialog = 0;
}

void
fe_message (char *msg, int wait)
{
	GtkWidget *dialog;

	dialog = gtkutil_simpledialog (msg);
	if (wait)
	{
		g_signal_connect (G_OBJECT (dialog), "destroy",
								G_CALLBACK (null_this_var), &dialog);
		while (dialog)
			gtk_main_iteration ();
	}
}

void
fe_set_topic (rage_session *sess, char *topic)
{
	if (!sess->gui->is_tab || sess == current_tab)
	{
		gtk_entry_set_text (GTK_ENTRY (sess->gui->topic_entry), topic);
		mg_set_topic_tip (sess);
	} else
	{
		if (sess->res->topic_text)
			free (sess->res->topic_text);
		sess->res->topic_text = strdup (topic);
	}
}

void
fe_set_hilight (rage_session *sess)
{
	if (sess->gui->is_tab)
		fe_set_tab_color (sess, 3, TRUE);	/* blue, with taskbar flash */
}

static void
fe_update_mode_entry (rage_session *sess, GtkWidget *entry, char **text, char *new_text)
{
	if (!sess->gui->is_tab || sess == current_tab)
	{
		if (sess->gui->flag_wid[0])	/* channel mode buttons enabled? */
			gtk_entry_set_text (GTK_ENTRY (entry), new_text);
	} else
	{
		if (sess->gui->is_tab)
		{
			if (*text)
				free (*text);
			*text = strdup (new_text);
		}
	}
}

void
fe_update_channel_key (rage_session *sess)
{
	fe_update_mode_entry (sess, sess->gui->key_entry,
								 &sess->res->key_text, sess->channelkey);
	fe_set_title (sess);
}

void
fe_update_channel_limit (rage_session *sess)
{
	char tmp[16];

	sprintf (tmp, "%d", sess->limit);
	fe_update_mode_entry (sess, sess->gui->limit_entry,
								 &sess->res->limit_text, tmp);
	fe_set_title (sess);
}

int
fe_is_chanwindow (struct server *serv)
{
	if (!serv->gui->chanlist_window)
		return 0;
	return 1;
}

int
fe_is_banwindow (rage_session *sess)
{
   if (!sess->res->banlist_window)
     return 0;
   return 1;
}

void
fe_chan_list_end (struct server *serv)
{
	gtk_widget_set_sensitive (serv->gui->chanlist_refresh, TRUE);
}

void
fe_notify_update (char *name)
{
	if (!name)
		notify_gui_update ();
}

void
fe_text_clear (rage_session *sess)
{
	gtk_xtext_clear (sess->res->buffer);
}

void
fe_close_window (rage_session *sess)
{
	if (sess->gui->is_tab)
		gtk_widget_destroy (sess->res->tab);
	else
		gtk_widget_destroy (sess->gui->window);
}

void
fe_progressbar_start (rage_session *sess)
{
	if (!sess->gui->is_tab || current_tab == sess)
	/* if it's the focused tab, create it for real! */
		mg_progressbar_create (sess->gui);
	else
	/* otherwise just remember to create on when it gets focused */
		sess->res->c_graph = TRUE;
}

void
fe_progressbar_end (server *serv)
{
	GSList *list = sess_list;
	rage_session *sess;

	while (list)				  /* check all windows that use this server and  *
									   * remove the connecting graph, if it has one. */
	{
		sess = list->data;
		if (sess->server == serv)
		{
			if (sess->gui->bar)
				mg_progressbar_destroy (sess->gui);
			sess->res->c_graph = FALSE;
		}
		list = list->next;
	}
}

void
fe_print_text (rage_session *sess, char *text)
{
	PrintTextRaw (sess->res->buffer, text, prefs.indent_nicks);

	if (!sess->new_data && sess != current_tab &&
		 sess->gui->is_tab && !sess->nick_said)
	{
		sess->new_data = TRUE;
		if (sess->msg_said)
			fe_set_tab_color (sess, 2, FALSE);
		else
			fe_set_tab_color (sess, 1, FALSE);
	}
}

void
fe_beep (void)
{
	gdk_beep ();
}

typedef struct {
	rage_session *sess;
	char *sstr;
} fe_lastlog_info;

static void
fe_lastlog_foreach (GtkXText *xtext, char *text, void *data)
{
	fe_lastlog_info *info = data;

	if (nocasestrstr (text, info->sstr))
		PrintText (info->sess, text);
}

void
fe_lastlog (rage_session *sess, rage_session *lastlog_sess, char *sstr)
{
	if (gtk_xtext_is_empty (sess->res->buffer))
		PrintText (lastlog_sess, _("Search buffer is empty.\n"));
	else
	{
		fe_lastlog_info info;

		info.sess = lastlog_sess;
		info.sstr = sstr;

		gtk_xtext_foreach (sess->res->buffer, (GtkXTextForeach)fe_lastlog_foreach, &info);
	}
}

void
fe_set_lag (server *serv, int lag)
{
	GSList *list = sess_list;
	rage_session *sess;
	gdouble per;
	char tip[64];
	unsigned long nowtim;

	if (lag == -1)
	{
		if (!serv->lag_sent)
			return;
		nowtim = make_ping_time ();
		lag = (nowtim - serv->lag_sent) / 100000;
	}

	per = (double)((double)lag / (double)40);
	if (per > 1.0)
		per = 1.0;

	snprintf (tip, sizeof (tip) - 1, "%s%d.%ds",
				 serv->lag_sent ? "+" : "", lag / 10, lag % 10);

	while (list)
	{
		sess = list->data;
		if (sess->server == serv)
		{
			if (!sess->gui->is_tab || current_tab == sess)
			{
				if (sess->gui->lagometer)
					gtk_progress_bar_set_fraction ((GtkProgressBar *) sess->gui->lagometer, per);
				if (sess->gui->laginfo)
					gtk_label_set_text ((GtkLabel *) sess->gui->laginfo, tip);
			} else
			{
				sess->res->lag_value = per;
				if (sess->res->lag_text)
					free (sess->res->lag_text);
				sess->res->lag_text = strdup (tip);
			}
		}
		list = list->next;
	}
}

void
fe_set_throttle (server *serv)
{
	GSList *list = sess_list;
	rage_session *sess;
	float per;
	char tbuf[64];

	per = (float) serv->sendq_len / 20.0;
	if (per > 1.0)
		per = 1.0;

	while (list)
	{
		sess = list->data;
		if (sess->server == serv)
		{
			snprintf (tbuf, sizeof (tbuf) - 1, _("%d lines"), serv->sendq_len);

			if (!sess->gui->is_tab || current_tab == sess)
			{
				if (sess->gui->throttlemeter)
					gtk_progress_bar_set_fraction ((GtkProgressBar *) sess->gui->throttlemeter, per);
				if (sess->gui->throttleinfo)
					gtk_label_set_text ((GtkLabel *) sess->gui->throttleinfo, tbuf);
			} else
			{
				sess->res->queue_value = per;
				if (sess->res->queue_text)
					free (sess->res->queue_text);
				sess->res->queue_text = strdup (tbuf);
			}
		}
		list = list->next;
	}
}

void
fe_ctrl_gui (rage_session *sess, int action, int arg)
{
	switch (action)
	{
	case 0:
		gtk_widget_hide (sess->gui->window); break;
	case 1:
		gtk_widget_show (sess->gui->window);
		gtk_window_present (GTK_WINDOW (sess->gui->window));
		break;
	case 2:
		mg_bring_tofront (sess->res->tab); break;
	case 3:
		fe_set_tab_color (sess, -1, TRUE); break; /* flash */
	case 4:
		fe_set_tab_color (sess, arg, FALSE); break;
	case 5:
		gtk_window_iconify (GTK_WINDOW (sess->gui->window)); break;
	}
}

void
fe_confirm (const char *message, void (*yesproc)(void *), void (*noproc)(void *), void *ud)
{
}

int
fe_gui_info (rage_session *sess, int info_type)
{
	switch (info_type)
	{
	case 0:	/* window status */
		if (!GTK_WIDGET_VISIBLE (GTK_WINDOW (sess->gui->window)))
			return 2;	/* hidden (iconified or systray) */
#if GTK_CHECK_VERSION(2,4,0)
		if (gtk_window_is_active (GTK_WINDOW (sess->gui->window)))
#else
#if GTK_CHECK_VERSION(2,2,0)
		if (GTK_WINDOW (sess->gui->window)->is_active)
#endif
#endif
			return 1;	/* active/focused */

		return 0;		/* normal (no keyboard focus or behind a window) */
	}

	return -1;
}

char *
fe_get_inputbox_contents (rage_session *sess)
{
	/* not the current tab */
	if (sess->res->input_text)
		return sess->res->input_text;

	/* current focused tab */
	return GTK_ENTRY (sess->gui->input_box)->text;
}

void
fe_set_inputbox_cursor (rage_session *sess, int delta, int pos)
{
	if (!sess->gui->is_tab || sess == current_tab)
	{
		if (delta)
			pos += gtk_editable_get_position (GTK_EDITABLE (sess->gui->input_box));
		gtk_editable_set_position (GTK_EDITABLE (sess->gui->input_box), pos);
	} else
	{
		/* we don't support changing non-front tabs yet */
	}
}

void
fe_set_inputbox_contents (rage_session *sess, char *text)
{
	if (!sess->gui->is_tab || sess == current_tab)
		gtk_entry_set_text (GTK_ENTRY (sess->gui->input_box), text);
	else
	{
		if (sess->res->topic_text)
			free (sess->res->topic_text);
		sess->res->topic_text = strdup (text);
	}
}

