#ifndef XCHAT_SERVER_H
#define XCHAT_SERVER_H

/* Common send commands */
inline void send_command(server *serv, char *target, char *buf);
inline void send_message(server *serv, char *target, char *buf);
inline void send_cmessage(server *serv, char *target, char *channel, char *buf);
inline void send_notice(server *serv, char *target, char *buf);
inline void send_cnotice(server *serv, char *target, char *channel, char *buf);
inline void send_reply(server *serv, char *target, char *buf);

/* Fomatted send commands */
void send_commandf(server *serv, char *target, char *fmt, ...);
void send_messagef(server *serv, char *target, char *fmt, ...);
void send_cmessagef(server *serv, char *target, char *channel, char *fmt, ...);
void send_noticef(server *serv, char *target, char *fmt, ...);
void send_cnoticef(server *serv, char *target, char *channel, char *fmt, ...);
void send_replyf(server *serv, char *target, char *fmt, ...);

/* Data manipulation */
void queue_target_change(server *serv, char *old_target, char *new_target);
void queue_remove_target(server *serv, char *target, int queue);
int queue_count(server *serv, char *target, int queue);
void queue_kill(server *serv);

void server_fill_her_up (server *serv);

#endif
