#ifndef RAGE_MENU_H
#define RAGE_MENU_H

GtkWidget *menu_create_main (void *accel_group, int bar, int away, int toplevel, GtkWidget **away_item, GtkWidget **user_menu);
void menu_newshell_set_palette (rage_session *sess);
void menu_urlmenu (GdkEventButton * event, char *url);
void menu_chanmenu (rage_session *sess, GdkEventButton * event, char *chan);
void menu_nickmenu (rage_session *sess, GdkEventButton * event, char *nick, int num_sel);
void menu_middlemenu (rage_session *sess, GdkEventButton *event);
void userlist_button_cb (GtkWidget * button, char *cmd);
void goto_url (char *url);
void nick_command_parse (rage_session *sess, char *cmd, char *nick, char *allnick);
void usermenu_update (void);
GtkWidget *menu_toggle_item (char *label, GtkWidget *menu, void *callback, void *userdata, int state);
GtkWidget *create_icon_menu (char *labeltext, void *stock_name, int is_stock);
void menu_create (GtkWidget *menu, GSList *list, char *target, int check_path);

#endif /* RAGE_MENU_H */
