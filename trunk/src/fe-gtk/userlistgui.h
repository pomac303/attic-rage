#ifndef RAGE_USERLISTGUI_H
#define RAGE_USERLISTGUI_H

void userlist_set_value (GtkWidget *treeview, gfloat val);
gfloat userlist_get_value (GtkWidget *treeview);
GtkWidget *userlist_create (GtkWidget *box);
void *userlist_create_model (void);
void userlist_show (rage_session *sess);
void userlist_select (rage_session *sess, char *name);
char **userlist_selection_list (GtkWidget *widget, int *num_ret);
GdkPixbuf *get_user_icon (server *serv, struct User *user);

#endif /* RAGE_USERLISTGUI_H */
