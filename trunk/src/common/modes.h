#ifndef XCHAT_MODES_H
#define XCHAT_MODES_H

int is_channel (server *serv, char *chan);
char get_nick_prefix (server *serv, unsigned int access);
unsigned int nick_access (server *serv, char *nick, int *modechars);
int mode_access (server *serv, char mode, char *prefix);
void inbound_005 (server *serv, int parc, char *parv[]);
void handle_mode (server *serv, int parc, char *parv[], char *nick, int numeric_324);
void send_channel_modes (session *sess, char *parv[], int start, int end, char sign, char mode);
char *get_isupport(server * serv, char *value);
int isupport(server * serv, char *value);
void run_005(server * serv);

#endif
