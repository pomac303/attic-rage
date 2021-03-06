#ifndef RAGE_MAINGUI_H
#define RAGE_MAINGUI_H

extern GtkStyle *input_style;
extern GtkWindow *parent_window;

void mg_changui_new (rage_session *sess, restore_gui *res, int tab, int focus);
void mg_update_xtext (GtkWidget *wid);
void mg_safe_quit (void);
void mg_switch_page (int relative, int num);
void mg_move_tab (GtkWidget *button, int delta);
void mg_move_tab_family (GtkWidget *button, int delta);
void mg_bring_tofront (GtkWidget *button);
void mg_userlist_showhide (rage_session *sess, int show);
void mg_chanmodebuttons_showhide (rage_session *sess, int show);
void mg_topic_showhide (rage_session *sess);
void mg_userlist_toggle (void);
void mg_set_topic_tip (rage_session *sess);
GtkWidget *mg_create_generic_tab (char *name, char *title, int force_toplevel, int link_buttons, void *close_callback, void *userdata, int width, int height, GtkWidget **vbox_ret, void *family);
void mg_set_title (GtkWidget *button, char *title);
void mg_set_access_icon (session_gui *gui, GdkPixbuf *pix);
void mg_apply_setup (void);
void mg_x_click_cb (GtkWidget *button, gpointer userdata);
void mg_link_cb (GtkWidget *but, gpointer userdata);
void mg_progressbar_create (session_gui *gui);
void mg_progressbar_destroy (session_gui *gui);

#endif /* RAGE_MAINGUI_H */
