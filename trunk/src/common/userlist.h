#ifndef XCHAT_USERLIST_H
#define XCHAT_USERLIST_H

struct User
{
	char nick[NICKLEN];
	char *hostname;
	char *realname;
	char *servername;
	time_t lasttalk;
	unsigned int access;	/* axs bit field */
	char prefix[2]; /* @ + % */
	unsigned int op:1;
	unsigned int hop:1;
	unsigned int voice:1;
	unsigned int me:1;
	unsigned int away:1;
	struct Membership *members; /* Which sessions this user is */
};

struct Membership
{
	struct Membership *prev_user;
	struct Membership *next_user;
	struct Membership *prev_session;
	struct Membership *next_session;	
	struct User *user;
	struct Session *session;
	/* Flags */
	unsigned int op:1;
	unsigned int hop:1;
	unsigned int voice:1;
	unsigned int away:1;
};

#define USERACCESS_SIZE 12

int userlist_add_hostname (rage_session *sess, char *nick,
											 char *hostname, char *realname,
											 char *servername, unsigned int away);

void userlist_set_away (rage_session *sess, char *nick, unsigned int away);
struct User *find_name (rage_session *sess, char *name);
struct User *find_name_global (struct server *serv, char *name);
void update_user_list (rage_session *sess);
void clear_user_list (rage_session *sess);
void free_userlist (rage_session *sess);
struct User* add_name (rage_session *sess, char *name, char *hostname);
int sub_name (rage_session *sess, char *name);
int change_nick (rage_session *sess, char *oldname, char *newname);
void ul_update_entry (rage_session *sess, char *name, char mode, char sign);
void update_all_of (char *name);
GSList *userlist_flat_list (rage_session *sess);
GList *userlist_double_list (rage_session *sess);
void userlist_rehash (rage_session *sess);
int find_ctarget (struct server *serv, char *channel, char *name);

#endif
