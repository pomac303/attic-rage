#ifndef XCHAT_SERVER_H
#define XCHAT_SERVER_H

/* The number of queues */
#define NR_OF_QUEUES 3

enum {
	QUEUE_COMMAND, QUEUE_MESSAGE, QUEUE_REPLY, QUEUE_NOTICE, 
	QUEUE_CMESSAGE, QUEUE_CNOTICE, QUEUE_MODE
};

/* Baisc message structure */
typedef struct queued_msg {
	char *msg;
	char *target;
	char *args;
	int type;
	int utf8:1;
} queued_msg;

/* Needed for the iterators */
struct it_check_data { char *old_target; char *new_target; };
struct it_remove_data { GQueue *queue; char *target; };
struct it_count_data {char *target; int count; };
struct it_mode_data {char *target; char *msg; char *args; int len; int max; };

/* Common send commands */
inline void send_command(server *serv, char *target, char *buf);
inline void send_message(server *serv, char *target, char *buf);
inline void send_cmessage(server *serv, char *target, char *channel, char *buf);
inline void send_notice(server *serv, char *target, char *buf);
inline void send_cnotice(server *serv, char *target, char *channel, char *buf);
inline void send_reply(server *serv, char *target, char *buf);
inline void send_mode(server *serv, char *target, char *mode);

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
#define queue_remove_replies(serv, target) queue_remove_target(serv, target, QUEUE_REPLY)
int queue_count(server *serv, char *target, int queue);
void queue_kill(server *serv);
void check_mjoin(rage_session *sess);

void server_fill_her_up (server *serv);

#endif
