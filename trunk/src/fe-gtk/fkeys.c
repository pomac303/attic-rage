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

static void replace_handle (GtkWidget * wid);

/***************** Key Binding Code ******************/

/* NOTES:

   To add a new action:
   1) inc KEY_MAX_ACTIONS
   2) write the function at the bottom of this file (with all the others)
   FIXME: Write about calling and returning
   3) Add it to key_actions

   --AGL

 */

/* Remember that the *number* of actions is this *plus* 1 --AGL */
#define KEY_MAX_ACTIONS 14
/* These are cp'ed from history.c --AGL */
#define STATE_SHIFT     GDK_SHIFT_MASK
#define	STATE_ALT	GDK_MOD1_MASK
#define STATE_CTRL	GDK_CONTROL_MASK

struct key_binding
{
	int keyval;						  /* GDK keynumber */
	char *keyname;					  /* String with the name of the function */
	int action;						  /* Index into key_actions */
	int mod;							  /* Flags of STATE_* above */
	char *data1, *data2;			  /* Pointers to strings, these must be freed */
	struct key_binding *next;
};

struct key_action
{
	int (*handler) (GtkWidget * wid, GdkEventKey * evt, char *d1, char *d2,
						 struct session * sess);
	char *name;
	char *help;
};

struct gcomp_data
{
	char data[CHANLEN];
	int elen;
};

static int key_load_kbs (char *);
static void key_load_defaults (void);
static void key_save_kbs (char *);
static int key_action_handle_command (GtkWidget * wid, GdkEventKey * evt,
												  char *d1, char *d2,
												  struct session *sess);
static int key_action_page_switch (GtkWidget * wid, GdkEventKey * evt,
											  char *d1, char *d2, struct session *sess);
int key_action_insert (GtkWidget * wid, GdkEventKey * evt, char *d1, char *d2,
							  struct session *sess);
static int key_action_scroll_page (GtkWidget * wid, GdkEventKey * evt,
											  char *d1, char *d2, struct session *sess);
static int key_action_set_buffer (GtkWidget * wid, GdkEventKey * evt,
											 char *d1, char *d2, struct session *sess);
static int key_action_history_up (GtkWidget * wid, GdkEventKey * evt,
											 char *d1, char *d2, struct session *sess);
static int key_action_history_down (GtkWidget * wid, GdkEventKey * evt,
												char *d1, char *d2, struct session *sess);
static int key_action_tab_comp (GtkWidget * wid, GdkEventKey * evt, char *d1,
										  char *d2, struct session *sess);
static int key_action_comp_chng (GtkWidget * wid, GdkEventKey * evt, char *d1,
                                                                                        char *d2, struct session *sess);
static int key_action_replace (GtkWidget * wid, GdkEventKey * evt, char *d1,
										 char *d2, struct session *sess);
static int key_action_move_tab_left (GtkWidget * wid, GdkEventKey * evt,
												 char *d1, char *d2,
												 struct session *sess);
static int key_action_move_tab_right (GtkWidget * wid, GdkEventKey * evt,
												  char *d1, char *d2,
												  struct session *sess);
static int key_action_move_tab_family_left (GtkWidget * wid, GdkEventKey * evt,
												 char *d1, char *d2,
												 struct session *sess);
static int key_action_move_tab_family_right (GtkWidget * wid, GdkEventKey * evt,
												  char *d1, char *d2,
												  struct session *sess);
static int key_action_put_history (GtkWidget * wid, GdkEventKey * evt,
												  char *d1, char *d2,
												  struct session *sess);

static GtkWidget *key_dialog;
static struct key_binding *keys_root = NULL;

static const struct key_action key_actions[KEY_MAX_ACTIONS + 1] = {

	{key_action_handle_command, "Run Command",
	 N_("The \002Run Command\002 action runs the data in Data 1 as if it has been typed into the entry box where you pressed the key sequence. Thus it can contain text (which will be sent to the channel/person), commands or user commands. When run all \002\\n\002 characters in Data 1 are used to deliminate seperate commands so it is possible to run more than one command. If you want a \002\\\002 in the actual text run then enter \002\\\\\002")},
	{key_action_page_switch, "Change Page",
	 N_("The \002Change Page\002 command switches between pages in the notebook. Set Data 1 to the page you want to switch to. If Data 2 is set to anything then the switch will be relative to the current position")},
	{key_action_insert, "Insert in Buffer",
	 N_("The \002Insert in Buffer\002 command will insert the contents of Data 1 into the entry where the key sequence was pressed at the current cursor position")},
	{key_action_scroll_page, "Scroll Page",
	 N_("The \002Scroll Page\002 command scrolls the text widget up or down one page or one line. Set Data 1 to either Up, Down, +1 or -1.")},
	{key_action_set_buffer, "Set Buffer",
	 N_("The \002Set Buffer\002 command sets the entry where the key sequence was entered to the contents of Data 1")},
	{key_action_history_up, "Last Command",
	 N_("The \002Last Command\002 command sets the entry to contain the last command entered - the same as pressing up in a shell")},
	{key_action_history_down, "Next Command",
	 N_("The \002Next Command\002 command sets the entry to contain the next command entered - the same as pressing down in a shell")},
	{key_action_tab_comp, "Complete nick/command",
	 N_("This command changes the text in the entry to finish an incomplete nickname or command. If Data 1 is set then double-tabbing in a string will select the last nick, not the next")},
	{key_action_comp_chng, "Change Selected Nick",
	 N_("This command scrolls up and down through the list of nicks. If Data 1 is set to anything it will scroll up, else it scrolls down")},
	{key_action_replace, "Check For Replace",
	 N_("This command checks the last word entered in the entry against the replace list and replaces it if it finds a match")},
	{key_action_move_tab_left, "Move front tab left",
	 N_("This command moves the front tab left by one")},
	{key_action_move_tab_right, "Move front tab right",
	 N_("This command moves the front tab right by one")},
	{key_action_move_tab_family_left, "Move tab family left",
	 N_("This command moves the current tab family to the left")},
	{key_action_move_tab_family_right, "Move tab family right",
	 N_("This command moves the current tab family to the right")},
	{key_action_put_history, "Push input line into history",
	 N_("Push input line into history but doesn't send to server")},
};

void
key_init ()
{
	keys_root = NULL;
	if (key_load_kbs (NULL) == 1)
	{
		key_load_defaults ();
		if (key_load_kbs (NULL) == 1)
			gtkutil_simpledialog
				(_("There was an error loading key bindings configuration"));
	}
}

static char *
key_get_key_name (int keyval)
{
	return gdk_keyval_name (gdk_keyval_to_lower (keyval));
}

/* Ok, here are the NOTES

   key_handle_key_press now handles all the key presses and history_keypress is
   now defunct. It goes thru the linked list keys_root and finds a matching
   key. It runs the action func and switches on these values:
   0) Return
   1) Find next
   2) stop signal and return

   * history_keypress is now dead (and gone)
   * key_handle_key_press now takes its role
   * All the possible actions are in a struct called key_actions (in fkeys.c)
   * it is made of {function, name, desc}
   * key bindings can pass 2 *text* strings to the handler. If more options are nee
   ded a format can be put on one of these options
   * key actions are passed {
   the entry widget
   the Gdk event
   data 1
   data 2
   session struct
   }
   * key bindings are stored in a linked list of key_binding structs
   * which looks like {
   int keyval;  GDK keynumber
   char *keyname;  String with the name of the function 
   int action;  Index into key_actions 
   int mod; Flags of STATE_* above 
   char *data1, *data2;  Pointers to strings, these must be freed 
   struct key_binding *next;
   }
   * remember that is (data1 || data2) != NULL then they need to be free()'ed

   --AGL

 */

gboolean
key_handle_key_press (GtkWidget *wid, GdkEventKey *evt, session *sess)
{
	struct key_binding *kb, *last = NULL;
	int keyval = evt->keyval;
	int mod, n;
	GSList *list;

	/* where did this event come from? */
	list = sess_list;
	while (list)
	{
		sess = list->data;
		if (sess->gui->input_box == wid)
		{
			if (sess->gui->is_tab)
				sess = current_tab;
			break;
		}
		list = list->next;
	}
	if (!list)
		return FALSE;
	current_sess = sess;

	mod = evt->state & (STATE_CTRL | STATE_ALT | STATE_SHIFT);

	kb = keys_root;
	while (kb)
	{
		if (kb->keyval == keyval && kb->mod == mod)
		{
			if (kb->action < 0 || kb->action > KEY_MAX_ACTIONS)
				return 0;

			/* Bump this binding to the top of the list */
			if (last != NULL)
			{
				last->next = kb->next;
				kb->next = keys_root;
				keys_root = kb;
			}
			/* Run the function */
			n = key_actions[kb->action].handler (wid, evt, kb->data1,
															 kb->data2, sess);
			switch (n)
			{
			case 0:
				return 1;
			case 2:
				gtk_signal_emit_stop_by_name (GTK_OBJECT (wid),
														"key_press_event");
				return 1;
			}
		}
		last = kb;
		kb = kb->next;
	}
	if (keyval == GDK_space)
		tab_clean ();

	/* check if it's a return or enter */
	/* ---handled by the "activate" signal in maingui.c */
/*	if ((evt->keyval == GDK_Return) || (evt->keyval == GDK_KP_Enter))
		mg_inputbox_cb (wid, sess);

	if (evt->keyval == GDK_KP_Enter)
		gtk_signal_emit_stop_by_name (GTK_OBJECT (wid), "key_press_event");*/

	return 0;
}

/* Walks keys_root and free()'s everything */
/*static void
key_free_all ()
{
	struct key_binding *cur, *next;

	cur = keys_root;
	while (cur)
	{
		next = cur->next;
		if (cur->data1)
			free (cur->data1);
		if (cur->data2)
			free (cur->data2);
		free (cur);
		cur = next;
	}
	keys_root = NULL;
}*/

/* Turns mod flags into a C-A-S string */
static char *
key_make_mod_str (int mod, char *buf)
{
	int i = 0;

	if (mod & STATE_CTRL)
	{
		if (i)
			buf[i++] = '-';
		buf[i++] = 'C';
	}
	if (mod & STATE_ALT)
	{
		if (i)
			buf[i++] = '-';
		buf[i++] = 'A';
	}
	if (mod & STATE_SHIFT)
	{
		if (i)
			buf[i++] = '-';
		buf[i++] = 'S';
	}
	buf[i] = 0;
	return buf;
}

/* ***** GUI code here ******************* */

/* NOTE: The key_dialog defin is above --AGL */
static GtkWidget *key_dialog_act_menu, *key_dialog_kb_clist;
static GtkWidget *key_dialog_tog_c, *key_dialog_tog_s, *key_dialog_tog_a;
static GtkWidget *key_dialog_ent_key, *key_dialog_ent_d1, *key_dialog_ent_d2;
static GtkWidget *key_dialog_text;

static void
key_load_defaults (void)
{
		/* This is the default config */
#define defcfg \
		"C\nPrior\nChange Page\nD1:-1\nD2:Relative\n\n"\
		"C\nNext\nChange Page\nD1:1\nD2:Relative\n\n"\
		"A\n9\nChange Page\nD1:9\nD2!\n\n"\
		"A\n8\nChange Page\nD1:8\nD2!\n\n"\
		"A\n7\nChange Page\nD1:7\nD2!\n\n"\
		"A\n6\nChange Page\nD1:6\nD2!\n\n"\
		"A\n5\nChange Page\nD1:5\nD2!\n\n"\
		"A\n4\nChange Page\nD1:4\nD2!\n\n"\
		"A\n3\nChange Page\nD1:3\nD2!\n\n"\
		"A\n2\nChange Page\nD1:2\nD2!\n\n"\
		"A\n1\nChange Page\nD1:1\nD2!\n\n"\
		"C\no\nInsert in Buffer\nD1:\nD2!\n\n"\
		"C\nb\nInsert in Buffer\nD1:\nD2!\n\n"\
		"C\nk\nInsert in Buffer\nD1:\nD2!\n\n"\
		"S\nNext\nChange Selected Nick\nD1!\nD2!\n\n"\
		"S\nPrior\nChange Selected Nick\nD1:Up\nD2!\n\n"\
		"None\nNext\nScroll Page\nD1:Down\nD2!\n\n"\
		"None\nPrior\nScroll Page\nD1:Up\nD2!\n\n"\
		"S\nDown\nScroll Page\nD1:+1\nD2!\n\n"\
		"S\nUp\nScroll Page\nD1:-1\nD2!\n\n"\
		"None\nDown\nNext Command\nD1!\nD2!\n\n"\
		"None\nUp\nLast Command\nD1!\nD2!\n\n"\
		"None\nTab\nComplete nick/command\nD1!\nD2!\n\n"\
		"None\nspace\nCheck For Replace\nD1!\nD2!\n\n"\
		"None\nReturn\nCheck For Replace\nD1!\nD2!\n\n"\
		"None\nKP_Enter\nCheck For Replace\nD1!\nD2!\n\n"\
		"C\nTab\nComplete nick/command\nD1:Up\nD2!\n\n"\
		"A\nLeft\nMove front tab left\nD1!\nD2!\n\n"\
		"A\nRight\nMove front tab right\nD1!\nD2!\n\n"\
		"CS\nPrior\nMove tab family left\nD1!\nD2!\n\n"\
		"CS\nNext\nMove tab family right\nD1!\nD2!\n\n"
	char buf[512];
	int fd;

	snprintf (buf, 512, "%s/keybindings.conf", get_xdir_fs ());
	fd = open (buf, O_CREAT | O_TRUNC | O_WRONLY | OFLAGS, 0x180);
	if (fd < 0)
		/* ???!!! */
		return;

	write (fd, defcfg, strlen (defcfg));
	close (fd);
}

static void
key_dialog_close (void)
{
	key_dialog = NULL;
	key_save_kbs (NULL);
}

static void
key_dialog_add_new (GtkWidget * button, GtkCList * list)
{
	gchar *strs[] = { "", NULL, NULL, NULL, NULL };
	struct key_binding *kb;

	strs[1] = _("<none>");
	strs[2] = _("<none>");
	strs[3] = _("<none>");
	strs[4] = _("<none>");

	kb = malloc (sizeof (struct key_binding));

	kb->keyval = 0;
	kb->keyname = NULL;
	kb->action = -1;
	kb->mod = 0;
	kb->data1 = kb->data2 = NULL;
	kb->next = keys_root;

	keys_root = kb;

	gtk_clist_set_row_data (GTK_CLIST (list),
									gtk_clist_append (GTK_CLIST (list), strs), kb);

}

static void
key_dialog_delete (GtkWidget * button, GtkCList * list)
{
	struct key_binding *kb, *cur, *last;
	int row = gtkutil_clist_selection ((GtkWidget *) list);

	if (row != -1)
	{
		kb = gtk_clist_get_row_data (list, row);
		cur = keys_root;
		last = NULL;
		while (cur)
		{
			if (cur == kb)
			{
				if (last)
					last->next = kb->next;
				else
					keys_root = kb->next;

				if (kb->data1)
					free (kb->data1);
				if (kb->data2)
					free (kb->data2);
				free (kb);
				gtk_clist_remove (list, row);
				return;
			}
			last = cur;
			cur = cur->next;
		}
		printf ("*** key_dialog_delete: couldn't find kb in list!\n");
		/*if (getenv ("XCHAT_DEBUG"))
			abort ();*/
	}
}

static void
key_dialog_sel_act (GtkWidget * un, int num)
{
	int row = gtkutil_clist_selection (key_dialog_kb_clist);
	struct key_binding *kb;

	if (row != -1)
	{
		kb = gtk_clist_get_row_data (GTK_CLIST (key_dialog_kb_clist), row);
		kb->action = num;
		gtk_clist_set_text (GTK_CLIST (key_dialog_kb_clist), row, 2,
								  _(key_actions[num].name));
		if (key_actions[num].help)
		{
			gtk_xtext_clear (GTK_XTEXT (key_dialog_text)->buffer);
			PrintTextRaw (GTK_XTEXT (key_dialog_text)->buffer, _(key_actions[num].help), 0);
		}
	}
}

static void
key_dialog_sel_row (GtkWidget * clist, gint row, gint column,
						  GdkEventButton * evt, gpointer data)
{
	struct key_binding *kb = gtk_clist_get_row_data (GTK_CLIST (clist), row);

	if (kb == NULL)
	{
		printf ("*** key_dialog_sel_row: kb == NULL\n");
		abort ();
	}
	if (kb->action > -1 && kb->action <= KEY_MAX_ACTIONS)
	{
		gtk_option_menu_set_history (GTK_OPTION_MENU (key_dialog_act_menu),
											  kb->action);
		if (key_actions[kb->action].help)
		{
			gtk_xtext_clear (GTK_XTEXT (key_dialog_text)->buffer);
			PrintTextRaw (GTK_XTEXT (key_dialog_text)->buffer, _(key_actions[kb->action].help), 0);
		}
	}
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (key_dialog_tog_c),
										  (kb->mod & STATE_CTRL) == STATE_CTRL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (key_dialog_tog_s),
										  (kb->mod & STATE_SHIFT) == STATE_SHIFT);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (key_dialog_tog_a),
										  (kb->mod & STATE_ALT) == STATE_ALT);

	if (kb->data1)
		gtk_entry_set_text (GTK_ENTRY (key_dialog_ent_d1), kb->data1);
	else
		gtk_entry_set_text (GTK_ENTRY (key_dialog_ent_d1), "");

	if (kb->data2)
		gtk_entry_set_text (GTK_ENTRY (key_dialog_ent_d2), kb->data2);
	else
		gtk_entry_set_text (GTK_ENTRY (key_dialog_ent_d2), "");

	if (kb->keyname)
		gtk_entry_set_text (GTK_ENTRY (key_dialog_ent_key), kb->keyname);
	else
		gtk_entry_set_text (GTK_ENTRY (key_dialog_ent_key), "");
}

static void
key_dialog_tog_key (GtkWidget * tog, int kstate)
{
	int state = GTK_TOGGLE_BUTTON (tog)->active;
	int row = gtkutil_clist_selection (key_dialog_kb_clist);
	struct key_binding *kb;
	char buf[32];

	if (row == -1)
		return;

	kb = gtk_clist_get_row_data (GTK_CLIST (key_dialog_kb_clist), row);
	if (state)
		kb->mod |= kstate;
	else
		kb->mod &= ~kstate;

	gtk_clist_set_text (GTK_CLIST (key_dialog_kb_clist), row, 0,
							  key_make_mod_str (kb->mod, buf));
}

static GtkWidget *
key_dialog_make_toggle (char *label, void *callback, void *option,
								GtkWidget * box)
{
	GtkWidget *wid;

	wid = gtk_check_button_new_with_label (label);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wid), 0);
	gtk_signal_connect (GTK_OBJECT (wid), "toggled",
							  GTK_SIGNAL_FUNC (callback), option);
	gtk_box_pack_end (GTK_BOX (box), wid, 0, 0, 0);
	gtk_widget_show (wid);

	return wid;
}

static GtkWidget *
key_dialog_make_entry (char *label, char *act, void *callback, void *option,
							  GtkWidget * box)
{
	GtkWidget *wid, *hbox;;

	hbox = gtk_hbox_new (0, 2);
	if (label)
	{
		wid = gtk_label_new (label);
		gtk_widget_show (wid);
		gtk_box_pack_start (GTK_BOX (hbox), wid, 0, 0, 0);
	}
	wid = gtk_entry_new ();
	if (act)
	{
		gtk_signal_connect (GTK_OBJECT (wid), act, GTK_SIGNAL_FUNC (callback),
								  option);
	}
	gtk_box_pack_start (GTK_BOX (hbox), wid, 0, 0, 0);
	gtk_widget_show (wid);
	gtk_widget_show (hbox);

	gtk_box_pack_start (GTK_BOX (box), hbox, 0, 0, 0);

	return wid;
}

static void
key_dialog_set_key (GtkWidget * entry, GdkEventKey * evt, void *none)
{
	int row = gtkutil_clist_selection (key_dialog_kb_clist);
	struct key_binding *kb;

	gtk_entry_set_text (GTK_ENTRY (entry), "");

	if (row == -1)
		return;

	kb = gtk_clist_get_row_data (GTK_CLIST (key_dialog_kb_clist), row);
	kb->keyval = evt->keyval;
	kb->keyname = key_get_key_name (kb->keyval);
	gtk_clist_set_text (GTK_CLIST (key_dialog_kb_clist), row, 1, kb->keyname);
	gtk_entry_set_text (GTK_ENTRY (entry), kb->keyname);
	gtk_signal_emit_stop_by_name (GTK_OBJECT (entry), "key_press_event");
}

static void
key_dialog_set_data (GtkWidget * entry, int d)
{
	const char *text = gtk_entry_get_text (GTK_ENTRY (entry));
	int row = gtkutil_clist_selection (key_dialog_kb_clist);
	struct key_binding *kb;
	char *buf;
	int len = strlen (text);

	len++;

	if (row == -1)
		return;

	kb = gtk_clist_get_row_data (GTK_CLIST (key_dialog_kb_clist), row);
	if (d == 0)
	{									  /* using data1 */
		if (kb->data1)
			free (kb->data1);
		buf = (char *) malloc (len);
		memcpy (buf, text, len);
		kb->data1 = buf;
		gtk_clist_set_text (GTK_CLIST (key_dialog_kb_clist), row, 3, text);
	} else
	{
		if (kb->data2)
			free (kb->data2);
		buf = (char *) malloc (len);
		memcpy (buf, text, len);
		kb->data2 = buf;
		gtk_clist_set_text (GTK_CLIST (key_dialog_kb_clist), row, 4, text);
	}
}

void
key_dialog_show ()
{
	GtkWidget *vbox, *hbox, *list, *vbox2, *wid, *wid2, *wid3, *hbox2;
	struct key_binding *kb;
	gchar *titles[] = { NULL, NULL, NULL, "1", "2" };
	char temp[32];
	int i;

	titles[0] = _("Mod");
	titles[1] = _("Key");
	titles[2] = _("Action");

	if (key_dialog)
	{
		mg_bring_tofront (key_dialog);
		return;
	}

	key_dialog =
			  mg_create_generic_tab ("editkeys", _("Rage: Keyboard Shortcuts"),
							TRUE, FALSE, key_dialog_close, NULL, 560, 330, &vbox, 0);

	hbox = gtk_hbox_new (0, 2);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, 1, 1, 0);

	list = gtkutil_clist_new (5, titles, hbox, 0, key_dialog_sel_row, 0, NULL,
									  0, GTK_SELECTION_SINGLE);
	gtk_widget_set_usize (list, 400, 0);
	key_dialog_kb_clist = list;

	gtk_widget_show (hbox);

	kb = keys_root;

	gtk_clist_set_column_width (GTK_CLIST (list), 1, 50);
	gtk_clist_set_column_width (GTK_CLIST (list), 2, 120);
	gtk_clist_set_column_width (GTK_CLIST (list), 3, 50);
	gtk_clist_set_column_width (GTK_CLIST (list), 4, 50);

	while (kb)
	{
		titles[0] = key_make_mod_str (kb->mod, temp);
		titles[1] = kb->keyname;
		if (kb->action < 0 || kb->action > KEY_MAX_ACTIONS)
			titles[2] = _("<none>");
		else
			titles[2] = key_actions[kb->action].name;
		if (kb->data1)
			titles[3] = kb->data1;
		else
			titles[3] = _("<none>");

		if (kb->data2)
			titles[4] = kb->data2;
		else
			titles[4] = _("<none>");

		gtk_clist_set_row_data (GTK_CLIST (list),
										gtk_clist_append (GTK_CLIST (list), titles),
										kb);

		kb = kb->next;
	}

	vbox2 = gtk_vbox_new (0, 2);
	gtk_box_pack_end (GTK_BOX (hbox), vbox2, 1, 1, 0);
	wid = gtk_button_new_with_label (_("Add new"));
	gtk_box_pack_start (GTK_BOX (vbox2), wid, 0, 0, 0);
	gtk_signal_connect (GTK_OBJECT (wid), "clicked",
							  GTK_SIGNAL_FUNC (key_dialog_add_new), list);
	gtk_widget_show (wid);
	wid = gtk_button_new_with_label (_("Delete"));
	gtk_box_pack_start (GTK_BOX (vbox2), wid, 0, 0, 0);
	gtk_signal_connect (GTK_OBJECT (wid), "clicked",
							  GTK_SIGNAL_FUNC (key_dialog_delete), list);
	gtk_widget_show (wid);
	gtk_widget_show (vbox2);

	wid = gtk_option_menu_new ();
	wid2 = gtk_menu_new ();

	for (i = 0; i <= KEY_MAX_ACTIONS; i++)
	{
		wid3 = gtk_menu_item_new_with_label (_(key_actions[i].name));
		gtk_widget_show (wid3);
		gtk_menu_shell_append (GTK_MENU_SHELL (wid2), wid3);
		gtk_signal_connect (GTK_OBJECT (wid3), "activate",
								  GTK_SIGNAL_FUNC (key_dialog_sel_act),
								  GINT_TO_POINTER (i));
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU (wid), wid2);
	gtk_option_menu_set_history (GTK_OPTION_MENU (wid), 0);
	gtk_box_pack_end (GTK_BOX (vbox2), wid, 0, 0, 0);
	gtk_widget_show (wid);
	key_dialog_act_menu = wid;

	key_dialog_tog_s = key_dialog_make_toggle (_("Shift"), key_dialog_tog_key,
															 (void *) STATE_SHIFT, vbox2);
	key_dialog_tog_a = key_dialog_make_toggle (_("Alt"), key_dialog_tog_key,
															 (void *) STATE_ALT, vbox2);
	key_dialog_tog_c = key_dialog_make_toggle (_("Ctrl"), key_dialog_tog_key,
															 (void *) STATE_CTRL, vbox2);

	key_dialog_ent_key = key_dialog_make_entry (_("Key"), "key_press_event",
															  key_dialog_set_key, NULL,
															  vbox2);

	key_dialog_ent_d1 = key_dialog_make_entry (_("Data 1"), "activate",
															 key_dialog_set_data, NULL,
															 vbox2);
	key_dialog_ent_d2 = key_dialog_make_entry (_("Data 2"), "activate",
															 key_dialog_set_data,
															 (void *) 1, vbox2);

	hbox2 = gtk_hbox_new (0, 2);
	gtk_box_pack_end (GTK_BOX (vbox), hbox2, 0, 0, 1);

	wid = gtk_xtext_new (colors, 0);
	gtk_xtext_set_tint (GTK_XTEXT (wid), prefs.tint_red, prefs.tint_green, prefs.tint_blue);
	gtk_xtext_set_background (GTK_XTEXT (wid),
									  channelwin_pix,
									  prefs.transparent, prefs.tint);
	gtk_widget_set_usize (wid, 0, 75);
	gtk_box_pack_start (GTK_BOX (hbox2), wid, 1, 1, 1);
	gtk_xtext_set_font (GTK_XTEXT (wid), prefs.font_normal);
	gtk_widget_show (wid);

	wid2 = gtk_vscrollbar_new (GTK_XTEXT (wid)->adj);
	gtk_box_pack_start (GTK_BOX (hbox2), wid2, 0, 0, 0);
	gtk_widget_show (wid2);

	gtk_widget_show (hbox2);
	key_dialog_text = wid;

	gtk_widget_show_all (key_dialog);
}

static void
key_save_kbs (char *fn)
{
	int fd, i;
	char buf[512];
	struct key_binding *kb;

	if (!fn)
		snprintf (buf, 510, "%s/keybindings.conf", get_xdir_fs ());
	else
	{
		safe_strcpy (buf, fn, sizeof (buf));
	}
	fd = open (buf, O_CREAT | O_TRUNC | O_WRONLY | OFLAGS, 0x180);
	if (fd < 0)
	{
		gtkutil_simpledialog (_("Error opening keys config file\n"));
		return;
	}
	write (fd, buf,
			 snprintf (buf, 510, "# Rage key bindings config file\n\n"));

	kb = keys_root;
	i = 0;

	while (kb)
	{
		if (kb->keyval == -1 || kb->keyname == NULL || kb->action < 0)
		{
			kb = kb->next;
			continue;
		}
		i = 0;
		if (kb->mod & STATE_CTRL)
		{
			i++;
			write (fd, "C", 1);
		}
		if (kb->mod & STATE_ALT)
		{
			i++;
			write (fd, "A", 1);
		}
		if (kb->mod & STATE_SHIFT)
		{
			i++;
			write (fd, "S", 1);
		}
		if (i == 0)
			write (fd, "None\n", 5);
		else
			write (fd, "\n", 1);

		write (fd, buf, snprintf (buf, 510, "%s\n%s\n", kb->keyname,
										  key_actions[kb->action].name));
		if (kb->data1 && kb->data1[0])
			write (fd, buf, snprintf (buf, 510, "D1:%s\n", kb->data1));
		else
			write (fd, "D1!\n", 4);

		if (kb->data2 && kb->data2[0])
			write (fd, buf, snprintf (buf, 510, "D2:%s\n", kb->data2));
		else
			write (fd, "D2!\n", 4);

		write (fd, "\n", 1);

		kb = kb->next;
	}

	close (fd);
}

/* I just know this is going to be a nasty parse, if you think it's bugged
   it almost certainly is so contact the XChat dev team --AGL */

static inline int
key_load_kbs_helper_mod (char *in, int *out)
{
	int n, len, mod = 0;

	/* First strip off the fluff */
	while (in[0] == ' ' || in[0] == '\t')
		in++;
	len = strlen (in);
	while (in[len] == ' ' || in[len] == '\t')
	{
		in[len] = 0;
		len--;
	}

	if (strcmp (in, "None") == 0)
	{
		*out = 0;
		return 0;
	}
	for (n = 0; n < len; n++)
	{
		switch (in[n])
		{
		case 'C':
			mod |= STATE_CTRL;
			break;
		case 'A':
			mod |= STATE_ALT;
			break;
		case 'S':
			mod |= STATE_SHIFT;
			break;
		default:
			return 1;
		}
	}

	*out = mod;
	return 0;
}

/* These are just local defines to keep me sane --AGL */

#define KBSTATE_MOD 0
#define KBSTATE_KEY 1
#define KBSTATE_ACT 2
#define KBSTATE_DT1 3
#define KBSTATE_DT2 4

/* *** Warning, Warning! - massive function ahead! --AGL */

static int
key_load_kbs (char *filename)
{
	char *buf, *ibuf;
	struct stat st;
	struct key_binding *kb = NULL, *last = NULL;
	int fd, len, pnt = 0, state = 0, n;

	buf = malloc (1000);
	if (filename == NULL)
		snprintf (buf, 1000, "%s/keybindings.conf", get_xdir_fs ());
	else
		strcpy (buf, filename);

	fd = open (buf, O_RDONLY | OFLAGS);
	free (buf);
	if (fd < 0)
		return 1;
	if (fstat (fd, &st) != 0)
		return 1;
	ibuf = malloc (st.st_size);
	read (fd, ibuf, st.st_size);
	close (fd);

	while (buf_get_line (ibuf, &buf, &pnt, st.st_size))
	{
		if (buf[0] == '#')
			continue;
		if (strlen (buf) == 0)
			continue;

		switch (state)
		{
		case KBSTATE_MOD:
			kb = (struct key_binding *) malloc (sizeof (struct key_binding));
			if (key_load_kbs_helper_mod (buf, &kb->mod))
				goto corrupt_file;
			state = KBSTATE_KEY;
			continue;
		case KBSTATE_KEY:
			/* First strip off the fluff */
			while (buf[0] == ' ' || buf[0] == '\t')
				buf++;
			len = strlen (buf);
			while (buf[len] == ' ' || buf[len] == '\t')
			{
				buf[len] = 0;
				len--;
			}

			n = gdk_keyval_from_name (buf);
			if (n == 0)
			{
				/* Unknown keyname, abort */
				if (last)
					last->next = NULL;
				free (ibuf);
				ibuf = malloc (1024);
				snprintf (ibuf, 1024,
							 _("Unknown keyname %s in key bindings config file\nLoad aborted, please fix %s/keybindings.conf\n"),
							 buf, get_xdir_utf8 ());
				gtkutil_simpledialog (ibuf);
				free (ibuf);
				return 2;
			}
			kb->keyname = gdk_keyval_name (n);
			kb->keyval = n;

			state = KBSTATE_ACT;
			continue;
		case KBSTATE_ACT:
			/* First strip off the fluff */
			while (buf[0] == ' ' || buf[0] == '\t')
				buf++;
			len = strlen (buf);
			while (buf[len] == ' ' || buf[len] == '\t')
			{
				buf[len] = 0;
				len--;
			}

			for (n = 0; n < KEY_MAX_ACTIONS + 1; n++)
			{
				if (strcmp (key_actions[n].name, buf) == 0)
				{
					kb->action = n;
					break;
				}
			}

			if (n == KEY_MAX_ACTIONS + 1)
			{
				if (last)
					last->next = NULL;
				free (ibuf);
				ibuf = malloc (1024);
				snprintf (ibuf, 1024,
							 _("Unknown action %s in key bindings config file\nLoad aborted, Please fix %s/keybindings\n"),
							 buf, get_xdir_utf8 ());
				gtkutil_simpledialog (ibuf);
				free (ibuf);
				return 3;
			}
			state = KBSTATE_DT1;
			continue;
		case KBSTATE_DT1:
		case KBSTATE_DT2:
			if (state == KBSTATE_DT1)
				kb->data1 = kb->data2 = NULL;

			while (buf[0] == ' ' || buf[0] == '\t')
				buf++;

			if (buf[0] != 'D')
			{
				free (ibuf);
				ibuf = malloc (1024);
				snprintf (ibuf, 1024,
							 _("Expecting Data line (beginning Dx{:|!}) but got:\n%s\n\nLoad aborted, Please fix %s/keybindings\n"),
							 buf, get_xdir_utf8 ());
				gtkutil_simpledialog (ibuf);
				free (ibuf);
				return 4;
			}
			switch (buf[1])
			{
			case '1':
				if (state != KBSTATE_DT1)
					goto corrupt_file;
				break;
			case '2':
				if (state != KBSTATE_DT2)
					goto corrupt_file;
				break;
			default:
				goto corrupt_file;
			}

			if (buf[2] == ':')
			{
				len = strlen (buf);
				/* Add one for the NULL, subtract 3 for the "Dx:" */
				len++;
				len -= 3;
				if (state == KBSTATE_DT1)
				{
					kb->data1 = malloc (len);
					memcpy (kb->data1, &buf[3], len);
				} else
				{
					kb->data2 = malloc (len);
					memcpy (kb->data2, &buf[3], len);
				}
			} else if (buf[2] == '!')
			{
				if (state == KBSTATE_DT1)
					kb->data1 = NULL;
				else
					kb->data2 = NULL;
			}
			if (state == KBSTATE_DT1)
			{
				state = KBSTATE_DT2;
				continue;
			} else
			{
				if (last)
					last->next = kb;
				else
					keys_root = kb;
				last = kb;

				state = KBSTATE_MOD;
			}

			continue;
		}
	}
	if (last)
		last->next = NULL;
	free (ibuf);
	return 0;

 corrupt_file:
	/*if (getenv ("XCHAT_DEBUG"))
		abort ();*/
	snprintf (ibuf, 1024,
						_("Key bindings config file is corrupt, load aborted\n"
								 "Please fix %s/keybindings.conf\n"),
						 get_xdir_utf8 ());
	gtkutil_simpledialog (ibuf);
	free (ibuf);
	return 5;
}

/* ***** Key actions start here *********** */

/* See the NOTES above --AGL */

/* "Run command" */
static int
key_action_handle_command (GtkWidget * wid, GdkEventKey * evt, char *d1,
									char *d2, struct session *sess)
{
	int ii, oi, len;
	char out[2048], d = 0;

	if (!d1)
		return 0;

	len = strlen (d1);

	/* Replace each "\n" substring with '\n' */
	for (ii = oi = 0; ii < len; ii++)
	{
		d = d1[ii];
		if (d == '\\')
		{
			ii++;
			d = d1[ii];
			if (d == 'n')
				out[oi++] = '\n';
			else if (d == '\\')
				out[oi++] = '\\';
			else
			{
				out[oi++] = '\\';
				out[oi++] = d;
			}
			continue;
		}
		out[oi++] = d;
	}
	out[oi] = 0;

	handle_multiline (sess, out, 0, 0);
	return 0;
}

static int
key_action_page_switch (GtkWidget * wid, GdkEventKey * evt, char *d1,
								char *d2, struct session *sess)
{
	int len, i, num;

	if (!d1)
		return 1;

	len = strlen (d1);
	if (!len)
		return 1;

	for (i = 0; i < len; i++)
	{
		if (d1[i] < '0' || d1[i] > '9')
		{
			if (i == 0 && (d1[i] == '+' || d1[i] == '-'))
				continue;
			else
				return 1;
		}
	}

	num = atoi (d1);
	if (!d2)
		num--;
	if (!d2 || d2[0] == 0)
		mg_switch_page (FALSE, num);
	else
		mg_switch_page (TRUE, num);
	return 0;
}

int
key_action_insert (GtkWidget * wid, GdkEventKey * evt, char *d1, char *d2,
						 struct session *sess)
{
	int tmp_pos;

	if (!d1)
		return 1;

	tmp_pos = gtk_editable_get_position (GTK_EDITABLE (wid));
	gtk_editable_insert_text (GTK_EDITABLE (wid), d1, strlen (d1), &tmp_pos);
	gtk_editable_set_position (GTK_EDITABLE (wid), tmp_pos);
	return 2;
}

/* handles PageUp/Down keys */
static int
key_action_scroll_page (GtkWidget * wid, GdkEventKey * evt, char *d1,
								char *d2, struct session *sess)
{
	int value, end;
	GtkAdjustment *adj;
	enum scroll_type { PAGE_UP, PAGE_DOWN, LINE_UP, LINE_DOWN };
	int type = PAGE_DOWN;

	if (d1)
	{
		if (!strcasecmp (d1, "up"))
			type = PAGE_UP;
		else if (!strcasecmp (d1, "+1"))
			type = LINE_DOWN;
		else if (!strcasecmp (d1, "-1"))
			type = LINE_UP;
	}

	if (!sess)
		return 0;

	adj = GTK_RANGE (sess->gui->vscrollbar)->adjustment;
	end = adj->upper - adj->lower - adj->page_size;

	switch (type)
	{
	case LINE_UP:
		value = adj->value - 1.0;
		break;

	case LINE_DOWN:
		value = adj->value + 1.0;
		break;

	case PAGE_UP:
		value = adj->value - (adj->page_size - 1);
		break;

	default:	/* PAGE_DOWN */
		value = adj->value + (adj->page_size - 1);
		break;
	}

	if (value < 0)
		value = 0;
	if (value > end)
		value = end;

	gtk_adjustment_set_value (adj, value);

	return 0;
}

static int
key_action_set_buffer (GtkWidget * wid, GdkEventKey * evt, char *d1, char *d2,
							  struct session *sess)
{
	if (!d1)
		return 1;
	if (d1[0] == 0)
		return 1;

	gtk_entry_set_text (GTK_ENTRY (wid), d1);
	gtk_editable_set_position (GTK_EDITABLE (wid), -1);

	return 2;
}

static int
key_action_history_up (GtkWidget * wid, GdkEventKey * ent, char *d1, char *d2,
							  struct session *sess)
{
	char *new_line;

	new_line = history_up (&sess->history, (char *)GTK_ENTRY (wid)->text);
	if (new_line)
	{
		gtk_entry_set_text (GTK_ENTRY (wid), new_line);
		gtk_editable_set_position (GTK_EDITABLE (wid), -1);
	}

	return 2;
}

static int
key_action_history_down (GtkWidget * wid, GdkEventKey * ent, char *d1,
								 char *d2, struct session *sess)
{
	char *new_line;

	new_line = history_down (&sess->history);
	if (new_line)
	{
		gtk_entry_set_text (GTK_ENTRY (wid), new_line);
		gtk_editable_set_position (GTK_EDITABLE (wid), -1);
	}

	return 2;
}

static int
key_action_tab_comp (GtkWidget *t, GdkEventKey *entry, char *d1, char *d2,
							struct session *sess)
{
	char buf[2048];
	int pos = 0, ret = 0;

	/* force the IM Context to reset */
	gtk_editable_set_editable (GTK_EDITABLE (t), FALSE);
	gtk_editable_set_editable (GTK_EDITABLE (t), TRUE);
	
	ret = tab_comp(sess, GTK_ENTRY (t)->text, buf, sizeof(buf), &pos, d1 && d1[0]); 

	if (buf[0] != 0)
	{
		gtk_entry_set_text (GTK_ENTRY (t), buf);
		gtk_editable_set_position (GTK_EDITABLE (t), 
				g_utf8_pointer_to_offset(buf, buf + pos));
	}
	return ret;
}
#undef COMP_BUF

static int
key_action_comp_chng (GtkWidget * wid, GdkEventKey * ent, char *d1, char *d2,
		struct session *sess)
{
	key_action_tab_comp(wid, ent, d1, d2, sess);
	return 2;
}


static int
key_action_replace (GtkWidget * wid, GdkEventKey * ent, char *d1, char *d2,
						  struct session *sess)
{
	replace_handle (wid);
	return 1;
}


static int
key_action_move_tab_left (GtkWidget * wid, GdkEventKey * ent, char *d1,
								  char *d2, struct session *sess)
{
	mg_move_tab (sess->res->tab, +1);
	return 2;						  /* don't allow default action */
}

static int
key_action_move_tab_right (GtkWidget * wid, GdkEventKey * ent, char *d1,
									char *d2, struct session *sess)
{
	mg_move_tab (sess->res->tab, -1);
	return 2;						  /* -''- */
}

static int
key_action_move_tab_family_left (GtkWidget * wid, GdkEventKey * ent, char *d1,
								  char *d2, struct session *sess)
{
	mg_move_tab_family (sess->res->tab, +1);
	return 2;						  /* don't allow default action */
}

static int
key_action_move_tab_family_right (GtkWidget * wid, GdkEventKey * ent, char *d1,
									char *d2, struct session *sess)
{
	mg_move_tab_family (sess->res->tab, -1);
	return 2;						  /* -''- */
}

static int
key_action_put_history (GtkWidget * wid, GdkEventKey * ent, char *d1,
									char *d2, struct session *sess)
{
	history_add (&sess->history, (char *)GTK_ENTRY (wid)->text);
	gtk_entry_set_text (GTK_ENTRY (wid), "");
	return 2;						  /* -''- */
}


/* -------- */


#define STATE_SHIFT	GDK_SHIFT_MASK
#define STATE_ALT		GDK_MOD1_MASK
#define STATE_CTRL	GDK_CONTROL_MASK

static void
replace_handle (GtkWidget *t)
{
	const char *text, *postfix_pnt;
	struct popup *pop;
	GSList *list = replace_list;
	char word[256];
	char postfix[256];
	char outbuf[4096];
	int c, len, xlen;

	text = gtk_entry_get_text (GTK_ENTRY (t));

	len = strlen (text);
	if (len < 1)
		return;

	for (c = len - 1; c > 0; c--)
	{
		if (text[c] == ' ')
			break;
	}
	if (text[c] == ' ')
		c++;
	xlen = c;
	if (len - c >= (sizeof (word) - 12))
		return;
	if (len - c < 1)
		return;
	memcpy (word, &text[c], len - c);
	word[len - c] = 0;
	len = strlen (word);
	if (word[0] == '\'' && word[len] == '\'')
		return;
	postfix_pnt = NULL;
	for (c = 0; c < len; c++)
	{
		if (word[c] == '\'')
		{
			postfix_pnt = &word[c + 1];
			word[c] = 0;
			break;
		}
	}

	if (postfix_pnt != NULL)
	{
		if (strlen (postfix_pnt) > sizeof (postfix) - 12)
			return;
		strcpy (postfix, postfix_pnt);
	}
	while (list)
	{
		pop = (struct popup *) list->data;
		if (strcmp (pop->name, word) == 0)
		{
			memcpy (outbuf, text, xlen);
			outbuf[xlen] = 0;
			if (postfix_pnt == NULL)
				snprintf (word, sizeof (word), "%s", pop->cmd);
			else
				snprintf (word, sizeof (word), "%s%s", pop->cmd, postfix);
			strcat (outbuf, word);
			gtk_entry_set_text (GTK_ENTRY (t), outbuf);
			gtk_editable_set_position (GTK_EDITABLE (t), -1);
			return;
		}
		list = list->next;
	}
}

