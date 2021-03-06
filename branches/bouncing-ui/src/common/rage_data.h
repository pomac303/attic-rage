/* NOTE: This file is DEPRECIATED.
** Please do not add stuff to it.
** This file will eventually become deleted.
** Cheers,
** Bart
*/

#ifndef XCHAT_H
#define XCHAT_H

#include <glib.h>
#include "history.h"
#ifndef WIN32
#include "../../config.h"
#endif

#ifndef HAVE_SNPRINTF
#define snprintf g_snprintf
#endif

#ifndef HAVE_VSNPRINTF
#define vsnprintf g_vsnprintf
#endif

#ifdef SOCKS
#ifdef __sgi
#include <sys/time.h>
#define INCLUDE_PROTOTYPES 1
#endif
#include <socks.h>
#endif

#ifdef USE_OPENSSL
#include <openssl/ssl.h>		  /* SSL_() */
#endif

#ifdef __EMX__						  /* for o/s 2 */
#define OFLAGS O_BINARY
#define strcasecmp stricmp
#define strncasecmp strnicmp
#define PATH_MAX MAXPATHLEN
#define FILEPATH_LEN_MAX MAXPATHLEN
#endif

#ifdef WIN32						/* for win32 */
#define OFLAGS O_BINARY
#define sleep(t) _sleep(t*1000)
#include <direct.h>
#define	F_OK	0
#define	X_OK	1
#define	W_OK	2
#define	R_OK	4
#ifndef S_ISDIR
#define	S_ISDIR(m)	((m) & _S_IFDIR)
#endif
#else									/* for unix */
#define OFLAGS 0
#endif

#define FONTNAMELEN	127
#define PATHLEN		255
#define DOMAINLEN	100
#define NICKLEN		64				/* including the NULL, so 63 really */
#define CHANLEN		300
#define PDIWORDS		32		/* Going away */
#define MAX_TOKENS	64

#define safe_strcpy(dest,src,len)	{stccpy(dest,src,len);}

#if defined(ENABLE_NLS) && !defined(_)
#  include <libintl.h>
#  define _(x) gettext(x)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#endif
#if !defined(_)
#  define N_(String) (String)
#  define _(x) (x)
#endif

struct nbexec
{
	int myfd;
	int childpid;
	int tochannel;					  /* making this int keeps the struct 4-byte aligned */
	int iotag;
	char *linebuf;
	int buffill;
	struct rage_session *sess;
};

struct rageprefs
{
	char nick1[NICKLEN];
	char nick2[NICKLEN];
	char nick3[NICKLEN];
	char realname[127];
	char username[127];
	char nick_suffix[4];				  /* Only ever holds a one-character string. */
	char awayreason[256];
	char quitreason[256];
	char partreason[256];
	char font_normal[FONTNAMELEN + 1];
#ifdef USE_ZVT
	char font_shell[FONTNAMELEN + 1];
#endif
	char doubleclickuser[256];
	char sounddir[PATHLEN + 1];
	char soundcmd[PATHLEN + 1];
	char background[PATHLEN + 1];
	char dccdir[PATHLEN + 1];
	char dcc_completed_dir[PATHLEN + 1];
	char bluestring[300];
	char dnsprogram[72];
	char hostname[127];
	char cmdchar[4];
	char logmask[256];
	char stamp_format[64];
	char timestamp_log_format[64];
	char irc_id_ytext[64];
	char irc_id_ntext[64];

	char proxy_host[64];
	int proxy_port;
	int proxy_type; /* 0=disabled, 1=wingate 2=socks4, 3=socks5, 4=http */
	unsigned int proxy_auth;
	char proxy_user[32];
	char proxy_pass[32];

	int first_dcc_send_port;
	int last_dcc_send_port;

	int tint_red;
	int tint_green;
	int tint_blue;

	int bomprefix;	/* BOM prefix utf8 lines? */
	int netsplit_time;
	int away_timeout;
	int away_size_max;
	int paned_pos;
	int tabs_position;
	int max_auto_indent;
	int dcc_blocksize;
	int max_lines;
	int notify_timeout;
	int dcctimeout;
	int dccstalltimeout;
	int dcc_global_max_get_cps;
	int dcc_global_max_send_cps;
	int dcc_max_get_cps;
	int dcc_max_send_cps;
	int mainwindow_left;
	int mainwindow_top;
	int mainwindow_width;
	int mainwindow_height;
	int gui_win_state;
	int dialog_left;
	int dialog_top;
	int dialog_width;
	int dialog_height;
	int dccpermissions;
	int recon_delay;
	int masktype;
	int userlist_sort;
	int nu_color;
	unsigned long local_ip;
	unsigned long dcc_ip;
	char dcc_ip_str[DOMAINLEN + 1];

	unsigned int tab_small;
	unsigned int tab_dnd;
	unsigned int tab_sort;
	unsigned int mainwindow_save;
	unsigned int perc_color;
	unsigned int perc_ascii;
	unsigned int use_trans;
	unsigned int autosave;
	unsigned int autodialog;
	unsigned int autosave_url;
	unsigned int autoreconnect;
	unsigned int autoreconnectonfail;
	unsigned int invisible;
	unsigned int servernotice;
	unsigned int wallops;
	unsigned int skipmotd;
	unsigned int autorejoin;
	unsigned int colorednicks;
	unsigned int chanmodebuttons;
	unsigned int userlistbuttons;
	unsigned int showhostname_in_userlist;
	unsigned int nickcompletion;
	unsigned int completion_amount;
	unsigned int tabchannels;
	unsigned int paned_userlist;
	unsigned int autodccchat;
	unsigned int autodccsend;
	unsigned int autoresume;
	unsigned int autoopendccsendwindow;
	unsigned int autoopendccrecvwindow;
	unsigned int autoopendccchatwindow;
	unsigned int transparent;
	unsigned int tint;
	unsigned int stripcolor;
	unsigned int timestamp;
	unsigned int fastdccsend;
	unsigned int dcc_send_fillspaces;
	unsigned int dcc_remove;
	unsigned int slist_skip;
	unsigned int slist_edit;
	unsigned int slist_select;
	unsigned int filterbeep;
	unsigned int beepmsg;
	unsigned int beepchans;
	unsigned int beephilight;
	unsigned int flash_hilight;
	unsigned int truncchans;
	unsigned int privmsgtab;
	unsigned int logging;
	unsigned int timestamp_logs;
	unsigned int newtabstofront;
	unsigned int dccwithnick;
	unsigned int hilitenotify;
	unsigned int hidever;
	unsigned int ip_from_server;
	unsigned int raw_modes;
	unsigned int show_away_once;
	unsigned int show_away_message;
	unsigned int auto_unmark_away;
	unsigned int away_track;
	unsigned int userhost;
	unsigned int use_server_tab;
	unsigned int snotices_tab;
	unsigned int notices_tab;
	unsigned int style_namelistgad;
	unsigned int style_inputbox;
	unsigned int windows_as_tabs;
	unsigned int indent_nicks;
	unsigned int show_marker;
	unsigned int show_separator;
	unsigned int thin_separator;
	unsigned int auto_indent;
	unsigned int wordwrap;
	unsigned int throttle;
	unsigned int fudgeservernotice;
	unsigned int topicbar;
	unsigned int hideuserlist;
	unsigned int hidemenu;
	unsigned int perlwarnings;
	unsigned int lagometer;
	unsigned int throttlemeter;
	unsigned int hebrew;
	unsigned int pingtimeout;
	unsigned int whois_on_notifyonline;
	unsigned int wait_on_exit;
	unsigned int confmode;
	unsigned int utf8_locale;
	unsigned int identd;

	unsigned int ctcp_number_limit;	/*flood */
	unsigned int ctcp_time_limit;	/*seconds of floods */

	unsigned int msg_number_limit;	/*same deal */
	unsigned int msg_time_limit;

	/* Tells us if we need to save, only when they've been edited.
		This is so that we continue using internal defaults (which can
		change in the next release) until the user edits them. */
	unsigned int save_pevents:1;
};

/* Session types */
#define SESS_SERVER	1
#define SESS_CHANNEL	2
#define SESS_DIALOG	3
#define SESS_NOTICES	4
#define SESS_SNOTICES	5

typedef struct rage_session
{
	struct server *server;
	struct Membership *members;	/** Members on this session */
	void *usertree_alpha;		/* pure alphabetical tree */
	void *usertree;			/* ordered with Ops first */
	struct User *me;		/* points to myself in the usertree */
	char channel[CHANLEN];
	char waitchannel[CHANLEN];	/* waiting to join this channel */
	char willjoinchannel[CHANLEN];	/* /join done for this channel */
	char channelkey[64];		/* XXX correct max length? */
	int limit;			/* channel user limit */
	int logfd;

	char lastnick[NICKLEN];		/* last nick you /msg'ed */

	/* Used for mass stacking */
	GQueue *stack_join;
	GQueue *stack_part;
	guint stack_timer;

	struct history history;
	int ops;			/* num. of ops in channel */
	int hops;			/* num. of half-oped users */
	int voices;			/* num. of voiced people */
	int total;			/* num. of users in channel */

	char *quitreason;
	char *topic;
	char *current_modes;		/* free() me */

	int mode_timeout_tag;

	struct rage_session *lastlog_sess;
	struct nbexec *running_exec;

	struct session_gui *gui;	/* initialized by fe_new_window */
	struct restore_gui *res;

	int userlisthidden;
	int type;
	int new_data:1;			/* new data avail? (purple tab) */
	int nick_said:1;		/* your nick mentioned? (blue tab) */
	int msg_said:1;			/* new msg available? (red tab) */
	int ignore_date:1;
	int ignore_mode:1;
	int ignore_names:1;
	int end_of_names:1;
	int doing_who:1;		/* /who sent on this channel */
	/* these are in the bottom-right menu */
	int hide_join_part:1;		/* hide join & part messages? */
	int beep:1;			/* beep enabled? */
	int color_paste:1;
	int done_away_check:1;		/* done checking for away status changes */
} rage_session;

typedef struct server
{
	/*  server control operations (in server*.c) */
	void (*connect)(struct server *, char *hostname, int port, int no_login);
	void (*disconnect)(rage_session *, int sendquit, int err);
	int  (*cleanup)(struct server *);
	void (*flush_queue)(struct server *);
	void (*auto_reconnect)(struct server *, int send_quit, int err);
	/* irc protocol functions (in proto*.c) */
	void (*p_inline)(struct server *, char *buf, int len);
	void (*p_invite)(struct server *, char *channel, char *nick);
	void (*p_cycle)(struct server *, char *channel, char *key);
	void (*p_ctcp)(struct server *, char *to, char *msg);
	void (*p_nctcp)(struct server *, char *to, char *msg);
	void (*p_quit)(struct server *, char *reason);
	void (*p_kick)(struct server *, char *channel, char *nick, char *reason);
	void (*p_part)(struct server *, char *channel, char *reason);
	void (*p_nickserv)(struct server *, char *pass);
	void (*p_join)(struct server *, char *channel, char *key);
	void (*p_login)(struct server *, char *user, char *realname);
	void (*p_join_info)(struct server *, char *channel);
	void (*p_mode)(struct server *, char *target, char *mode);
	void (*p_user_list)(struct server *, char *channel);
	void (*p_away_status)(struct server *, char *channel);
	void (*p_whois)(struct server *, char *nicks);
	void (*p_get_ip)(struct server *, char *nick);
	void (*p_get_ip_uh)(struct server *, char *nick);
	void (*p_set_back)(struct server *);
	void (*p_set_away)(struct server *, char *reason);
	void (*p_message)(struct server *, char *channel, char *text);
	void (*p_action)(struct server *, char *channel, char *act);
	void (*p_notice)(struct server *, char *channel, char *text);
	void (*p_topic)(struct server *, char *channel, char *topic);
	void (*p_list_channels)(struct server *, char *arg);
	void (*p_change_nick)(struct server *, char *new_nick);
	void (*p_names)(struct server *, char *channel);
	void (*p_ping)(struct server *, char *to, char *timestring);
/*	void (*p_set_away)(struct server *);*/
	int (*p_raw)(struct server *, char *raw);
	int (*p_cmp)(const char *s1, const char *s2);

	int port;
	int sok;				/* is equal to sok4 or sok6 (the one we are using) */
	int sok4;				/* tcp4 socket */
	int sok6;				/* tcp6 socket */
	int id;					/* unique ID number (for plugin API) */
#ifdef USE_OPENSSL
	SSL *ssl;
	int ssl_do_connect_tag;
#endif
	int childread;
	int childwrite;
	int childpid;
	int iotag;
	int recondelay_tag;			/* reconnect delay timeout */
	char hostname[128];			/* real ip number */
	char servername[128];			/* what the server says is its name */
	char password[86];
	char nick[NICKLEN];
	char linebuf[522];			/* RFC says 512 including \r\n */
	char *last_away_reason;
	int pos;				/* current position in linebuf */
	int nickcount;
	int nickservtype;			/* 0=/MSG nickserv 1=/NICKSERV 2=/NS */

	void *network;				/* points to entry in servlist.c or NULL! */

	/* things relating to the output queue including leaky bucket
	 * throttling and the timer. */
	time_t queue_time;
	int queue_level;
	guint queue_timer;
	GQueue *out_queue[3];

	/* Netsplit detection and other magic */
	GQueue *split_queue;
	guint split_timer;
	char *split_reason;
	
	time_t next_send;			/* cptr->since in ircu */
	time_t prev_now;			/* previous now-time */
	int sendq_len;				/* queue size */

	rage_session *front_session;		/* front-most window/tab */
	rage_session *server_session;		/* server window/tab */

	struct server_gui *gui;		  	/* initialized by fe_new_server */

	unsigned int ctcp_counter;	  	/*flood */
	time_t ctcp_last_time;

	unsigned int msg_counter;	  	/*counts the msg tab opened in a certain time */
	time_t msg_last_time;

	/*time_t connect_time;*/		/* when did it connect? */
	time_t lag_sent;
	time_t ping_recv;			/* when we last got a ping reply */
	time_t away_time;			/* when we were marked away */

	char *encoding;				/* NULL for system */
	dict_t isupport;

	int motd_skipped:1;
	unsigned int connected:1;
	unsigned int connecting:1;
	int no_login:1;
	int skip_next_userhost:1;		/* used for "get my ip from server" */
	int inside_whois:1;
	int doing_dns:1;			/* /dns has been done */
	unsigned int end_of_motd:1;		/* end of motd reached (logged in) */
	int sent_quit:1;			/* sent a QUIT already? */
	int use_listargs:1;			/* undernet and dalnet need /list >0,<10000 */
	unsigned int is_away:1;
	int reconnect_away:1;			/* whether to reconnect in is_away state */
	int dont_use_proxy:1;			/* to proxy or not to proxy */
	int bad_prefix:1;			/* gave us a bad PREFIX= 005 number */
	int use_who:1;				/* whether to use WHO command to get dcc_ip */
#ifdef USE_OPENSSL
	int use_ssl:1;				/* is server SSL capable? */
	int accept_invalid_cert:1;		/* ignore result of server's cert. verify */
#endif
} server;

typedef int (*cmd_callback) (rage_session * sess, char *cmd, char *buf);

typedef struct command
{
	char *name;
	cmd_callback callback;
	char needserver;
	char needchannel;
	char *help;
} command;

typedef int (*ircparser_numeric_callback) (rage_session *sess, int parc, char *parv[]);

typedef struct ircparser_numeric
{
	int numeric;
	ircparser_numeric_callback callback;
} ircparser_numeric;

typedef int (*ircparser_server_callback) 
	(rage_session *sess, int parc, char *parv[], char *ip, char *nick, int is_server);

typedef struct ircparser_server
{
	char *name;
	ircparser_server_callback callback;
} ircparser_server;

struct away_msg
{
	struct server *server;
	char nick[NICKLEN];
	char *message;
};

/* not just for popups, but used for usercommands, ctcp replies,
   userlist buttons etc */

struct popup
{
	char *cmd;
	char *name;
};

#endif
