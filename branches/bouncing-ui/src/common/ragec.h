#ifndef XCHAT_C_H
#define XCHAT_C_H

extern struct rageprefs prefs;

extern int auto_connect;
extern int skip_plugins;
extern int xchat_is_quitting;
extern char *connect_url;

extern struct rage_session *current_sess;
extern struct rage_session *current_tab;

extern GSList *popup_list;
extern GSList *button_list;
extern GSList *dlgbutton_list;
extern dict_t command_list;
extern dict_t ctcp_list;
extern GSList *replace_list;
extern GSList *sess_list;
extern GSList *serv_list;
extern GSList *dcc_list;
extern GSList *ignore_list;
extern GSList *usermenu_list;
extern GSList *urlhandler_list;
extern GSList *tabmenu_list;

rage_session * find_channel (server *serv, char *chan);
rage_session * find_dialog (server *serv, char *nick);
rage_session * new_ircwindow (server *serv, char *name, int type, int focus);
void set_server_defaults (server *serv);
struct away_msg *find_away_message (struct server *serv, char *nick);
void save_away_message (server *serv, char *nick, char *msg);
int is_server (server * serv);
int is_session (rage_session * sess);
void lag_check (void);
void kill_session_callback (rage_session * killsess);
void xchat_exit (void);
void xchat_exec (char *cmd);
char *get_network (rage_session *sess, gboolean fallback);

#endif
