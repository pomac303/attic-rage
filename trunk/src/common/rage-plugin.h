/* You can distribute this header with your plugins for easy compilation */
#ifndef RAGE_PLUGIN_H
#define RAGE_PLUGIN_H

#define RAGE_IFACE_MAJOR	1
#define RAGE_IFACE_MINOR	0
#define RAGE_IFACE_MICRO	0
#define RAGE_IFACE_VERSION	((RAGE_IFACE_MAJOR * 10000) + \
				 (RAGE_IFACE_MINOR * 100) + \
				 (RAGE_IFACE_MICRO))

#define RAGE_PRI_HIGHEST	127
#define RAGE_PRI_HIGH		64
#define RAGE_PRI_NORM		0
#define RAGE_PRI_LOW		(-64)
#define RAGE_PRI_LOWEST	(-128)

#define RAGE_FD_READ		1
#define RAGE_FD_WRITE		2
#define RAGE_FD_EXCEPTION	4
#define RAGE_FD_NOTSOCKET	8

#define RAGE_EAT_NONE		0	/* pass it on through! */
#define RAGE_EAT_RAGE		1	/* don't let rage see this event */
#define RAGE_EAT_PLUGIN	2	/* don't let other plugins see this event */
#define RAGE_EAT_ALL		(RAGE_EAT_RAGE|RAGE_EAT_PLUGIN)	
				/* don't let anything see this event */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _rage_plugin rage_plugin;
typedef struct _rage_list rage_list;
typedef struct _rage_hook rage_hook;
#ifdef RAGE_INTERNAL
typedef rage_session rage_context;
#else
typedef struct _rage_dummy_context rage_context;
#endif

typedef int (rage_cmd_cb) (int parc, char *parv[], void *user_data);
typedef int (rage_serv_cb) (int parc, char *parv[], void *user_data);
typedef int (rage_print_cb) (int parc, char *parv[], void *user_data);
typedef int (rage_fd_cb) (int fd, int flags, void *user_data);
typedef int (rage_timer_cb) (void *user_data);
typedef int (rage_init_func) (rage_plugin *, char **, char **, char **, char *);
typedef int (rage_deinit_func) (rage_plugin *);

struct _rage_plugin
{
	/* parv[0] the command
	 * parv[1] the entire line
	 */
	rage_hook *(*rage_hook_command) (rage_plugin *ph,
		    const char *name,
		    int pri,
		    rage_cmd_cb *callb,
		    const char *help_text,
		    void *userdata);
	rage_hook *(*rage_hook_server) (rage_plugin *ph,
		   const char *name,
		   int pri,
		   rage_serv_cb *callb,
		   void *userdata);
	rage_hook *(*rage_hook_print) (rage_plugin *ph,
		  const char *name,
		  int pri,
		  rage_print_cb *callb,
		  void *userdata);
	rage_hook *(*rage_hook_timer) (rage_plugin *ph,
		  int timeout,
		  rage_timer_cb *callb,
		  void *userdata);
	rage_hook *(*rage_hook_fd) (rage_plugin *ph,
		   int fd,
		   int flags,
		   rage_fd_cb *callb,
		   void *userdata);
	void *(*rage_unhook) (rage_plugin *ph,
	      rage_hook *hook);
	void (*rage_print) (rage_plugin *ph,
	     const char *text);
	void (*rage_printf) (rage_plugin *ph,
	      const char *format, ...);
	void (*rage_command) (rage_plugin *ph,
	       const char *command);
	void (*rage_commandf) (rage_plugin *ph,
		const char *format, ...);
	int (*rage_nickcmp) (rage_plugin *ph,
	       const char *s1,
	       const char *s2);
	int (*rage_set_context) (rage_plugin *ph,
		rage_context *ctx);
	rage_context *(*rage_find_context) (rage_plugin *ph,
		    const char *servname,
		    const char *channel);
	rage_context *(*rage_get_context) (rage_plugin *ph);
	const char *(*rage_get_info) (rage_plugin *ph,
		const char *id);
	int (*rage_get_prefs) (rage_plugin *ph,
		 const char *name,
		 const char **string,
		 int *integer);
	rage_list * (*rage_list_get) (rage_plugin *ph,
		const char *name);
	void (*rage_list_free) (rage_plugin *ph,
		 rage_list *xlist);
	const char ** (*rage_list_fields) (rage_plugin *ph,
		   const char *name);
	int (*rage_list_next) (rage_plugin *ph,
		 rage_list *xlist);
	const char * (*rage_list_str) (rage_plugin *ph,
		rage_list *xlist,
		const char *name);
	int (*rage_list_int) (rage_plugin *ph,
		rage_list *xlist,
		const char *name);
	void * (*rage_plugingui_add) (rage_plugin *ph,
		     const char *filename,
		     const char *name,
		     const char *desc,
		     const char *version,
		     char *reserved);
	void (*rage_plugingui_remove) (rage_plugin *ph,
			void *handle);
	int (*rage_emit_print) (rage_plugin *ph,
			const char *event_name, ...);
	int (*rage_read_fd) (rage_plugin *ph,
			void *src,
			char *buf,
			int *len);
	time_t (*rage_list_time) (rage_plugin *ph,
		rage_list *xlist,
		const char *name);
	char *(*rage_gettext) (rage_plugin *ph,
		const char *msgid);
	void (*rage_send_modes) (rage_plugin *ph,
		  const char **targets,
		  int ntargets,
		  char sign,
		  char mode);
/* These are things used internally by rage, you shouldn't modify them,
 * They are only defined if RAGE_INTERNAL is defined (which should only
 * be defined internally to rage 
 */
#ifdef RAGE_INTERNAL
	/* Dummy padding to deal with version skew */
	void *(*rage_dummy6) (rage_plugin *ph);
	void *(*rage_dummy5) (rage_plugin *ph);
	void *(*rage_dummy4) (rage_plugin *ph);
	void *(*rage_dummy3) (rage_plugin *ph);
	void *(*rage_dummy2) (rage_plugin *ph);
	void *(*rage_dummy1) (rage_plugin *ph);
	/* PRIVATE FIELDS! */
	void *handle;		/* from dlopen */
	char *filename;	/* loaded from */
	char *name;
	char *desc;
	char *version;
	rage_session *context;
	void *deinit_callback;	/* pointer to rage_plugin_deinit */
	unsigned int fake:1;		/* fake plugin. Added by rage_plugingui_add() */
	
#endif
};

rage_hook *
rage_hook_command (rage_plugin *ph,
		    const char *name,
		    int pri,
		    rage_cmd_cb *callb,
		    const char *help_text,
		    void *userdata);

rage_hook *
rage_hook_server (rage_plugin *ph,
		   const char *name,
		   int pri,
		   rage_cmd_cb *callb,
		   void *userdata);

rage_hook *
rage_hook_print (rage_plugin *ph,
		  const char *name,
		  int pri,
		  int (*callback) (int parc, char *parv[], void *user_data),
		  void *userdata);

rage_hook *
rage_hook_timer (rage_plugin *ph,
		  int timeout,
		  int (*callback) (void *user_data),
		  void *userdata);

rage_hook *
rage_hook_fd (rage_plugin *ph,
		int fd,
		int flags,
		int (*callback) (int fd, int flags, void *user_data),
		void *userdata);

void *
rage_unhook (rage_plugin *ph,
	      rage_hook *hook);

void
rage_print (rage_plugin *ph,
	     const char *text);

void
rage_printf (rage_plugin *ph,
	      const char *format, ...);

void
rage_command (rage_plugin *ph,
	       const char *command);

void
rage_commandf (rage_plugin *ph,
		const char *format, ...);

int
rage_nickcmp (rage_plugin *ph,
	       const char *s1,
	       const char *s2);

int
rage_set_context (rage_plugin *ph,
		   rage_context *ctx);

rage_context *
rage_find_context (rage_plugin *ph,
		    const char *servname,
		    const char *channel);

rage_context *
rage_get_context (rage_plugin *ph);

const char *
rage_get_info (rage_plugin *ph,
		const char *id);

int
rage_get_prefs (rage_plugin *ph,
		 const char *name,
		 const char **string,
		 int *integer);

rage_list *
rage_list_get (rage_plugin *ph,
		const char *name);

void
rage_list_free (rage_plugin *ph,
		 rage_list *xlist);

const char **
rage_list_fields (rage_plugin *ph,
		   const char *name);

int
rage_list_next (rage_plugin *ph,
		 rage_list *xlist);

const char *
rage_list_str (rage_plugin *ph,
		rage_list *xlist,
		const char *name);

int
rage_list_int (rage_plugin *ph,
		rage_list *xlist,
		const char *name);

time_t
rage_list_time (rage_plugin *ph,
		 rage_list *xlist,
		 const char *name);

void *
rage_plugingui_add (rage_plugin *ph,
		     const char *filename,
		     const char *name,
		     const char *desc,
		     const char *version,
		     char *reserved);

void
rage_plugingui_remove (rage_plugin *ph,
			void *handle);

int 
rage_emit_print (rage_plugin *ph,
		  const char *event_name, ...);

char *
rage_gettext (rage_plugin *ph,
	       const char *msgid);

void
rage_send_modes (rage_plugin *ph,
		  const char **targets,
		  int ntargets,
		  char sign,
		  char mode);

#if !defined(PLUGIN_C) && defined(WIN32)
#ifndef RAGE_PLUGIN_HANDLE
#define RAGE_PLUGIN_HANDLE (ph)
#endif
#define rage_hook_command ((RAGE_PLUGIN_HANDLE)->rage_hook_command)
#define rage_hook_server ((RAGE_PLUGIN_HANDLE)->rage_hook_server)
#define rage_hook_print ((RAGE_PLUGIN_HANDLE)->rage_hook_print)
#define rage_hook_timer ((RAGE_PLUGIN_HANDLE)->rage_hook_timer)
#define rage_hook_fd ((RAGE_PLUGIN_HANDLE)->rage_hook_fd)
#define rage_unhook ((RAGE_PLUGIN_HANDLE)->rage_unhook)
#define rage_print ((RAGE_PLUGIN_HANDLE)->rage_print)
#define rage_printf ((RAGE_PLUGIN_HANDLE)->rage_printf)
#define rage_command ((RAGE_PLUGIN_HANDLE)->rage_command)
#define rage_commandf ((RAGE_PLUGIN_HANDLE)->rage_commandf)
#define rage_nickcmp ((RAGE_PLUGIN_HANDLE)->rage_nickcmp)
#define rage_set_context ((RAGE_PLUGIN_HANDLE)->rage_set_context)
#define rage_find_context ((RAGE_PLUGIN_HANDLE)->rage_find_context)
#define rage_get_context ((RAGE_PLUGIN_HANDLE)->rage_get_context)
#define rage_get_info ((RAGE_PLUGIN_HANDLE)->rage_get_info)
#define rage_get_prefs ((RAGE_PLUGIN_HANDLE)->rage_get_prefs)
#define rage_list_get ((RAGE_PLUGIN_HANDLE)->rage_list_get)
#define rage_list_free ((RAGE_PLUGIN_HANDLE)->rage_list_free)
#define rage_list_fields ((RAGE_PLUGIN_HANDLE)->rage_list_fields)
#define rage_list_str ((RAGE_PLUGIN_HANDLE)->rage_list_str)
#define rage_list_int ((RAGE_PLUGIN_HANDLE)->rage_list_int)
#define rage_list_time ((RAGE_PLUGIN_HANDLE)->rage_list_time)
#define rage_list_next ((RAGE_PLUGIN_HANDLE)->rage_list_next)
#define rage_plugingui_add ((RAGE_PLUGIN_HANDLE)->rage_plugingui_add)
#define rage_plugingui_remove ((RAGE_PLUGIN_HANDLE)->rage_plugingui_remove)
#define rage_emit_print ((RAGE_PLUGIN_HANDLE)->rage_emit_print)
#define rage_gettext ((RAGE_PLUGIN_HANDLE)->rage_gettext)
#define rage_send_modes ((RAGE_PLUGIN_HANDLE)->rage_send_modes)
#endif

#ifdef __cplusplus
}
#endif

#include "plugin-timer.h"

#endif
