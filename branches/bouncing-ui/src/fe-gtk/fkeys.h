#ifndef RAGE_FKEYS_H
#define RAGE_FKEYS_H

void key_init (void);
void key_dialog_show (void);
int key_handle_key_press (GtkWidget * wid, GdkEventKey * evt, rage_session *sess);
int key_action_insert (GtkWidget * wid, GdkEventKey * evt, char *d1, char *d2,
						 rage_session *sess);

#endif /* RAGE_FKEYS_H */
