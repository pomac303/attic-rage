#ifndef XCHAT_OUTBOUND_H
#define XCHAT_OUTBOUND_H

int load_trans_table (char *full_path);
int auto_insert (char *dest, int destlen, unsigned char *src, int parc, char *parv[],
				 char *a, char *c, char *d, char *h, char *n, char *s);
int handle_command (rage_session *sess, char *cmd, int check_spch);
void process_data_init (char *buf, char *cmd, int parc,
						 char *parv[], int handle_quotes);
void handle_multiline (rage_session *sess, char *cmd, int history, int nocommand);
void check_special_chars (char *cmd, int do_ascii);
void notc_msg (rage_session *sess);
void server_sendpart (server * serv, char *channel, char *reason);
void server_sendquit (rage_session * sess);
void setup_commands(void);
GList *get_command_list(GList *list);

#endif
