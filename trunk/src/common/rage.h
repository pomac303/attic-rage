#ifndef _RAGE_COMMON_H
#define _RAGE_COMMON_H
/*
** rage.h
**
** Rage's super include, handling platform dependancies and specific
** requirements.
**
** (c) 2004 The Rage Development Team
*/

#define _FILE_OFFSET_BITS 64

/* Include standard C includes */
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#ifndef WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
//#include <sys/time.h>
#include <sys/utsname.h>
#include <dirent.h>
#endif

/* Require glib */
#include <glib/gslist.h>
#include <glib/glist.h>
#include <glib/gutils.h>
#include <glib/giochannel.h>
#include <glib/gstrfuncs.h>
#include <glib/ghash.h>
#ifndef HAVE_SNPRINTF
#define snprintf g_snprintf
#endif

/* Platform specifics */
#ifdef WIN32
#include "rage-win32.h"
#endif

/* If we're debugging, enable some debug code */
#if !defined(NDEBUG) || defined(USE_DEBUG)
#define malloc(n) DBUG_malloc(n, __FILE__, __LINE__)
#define realloc(n, m) DBUG_realloc(n, m, __FILE__, __LINE__)
#define free(n) DBUG_free(n, __FILE__, __LINE__)
#define strdup(n) DBUG_strdup(n, __FILE__, __LINE__)
void *DBUG_malloc (int size, char *file, int line);
void *DBUG_strdup (char *str, char *file, int line);
void DBUG_free (void *buf, char *file, int line);
void *DBUG_realloc (char *old, int len, char *file, int line);
/* don't forget to link with debug.c */
#endif

/* TODO: Include xchat.h. Note this will disappear soon! */
#include "dict.h"
#include "xchat.h"

/* Include Internet compatibility stuff */
#define WANTSOCKET
#define WANTARPA
#define WANTDNS
#include "inet.h"

/* Include common includes */
/* Bletch! */
#include "xchat-plugin.h"
#include "cfgfiles.h"
#include "proto-irc.h"
#include "util.h"
#include "fe.h"
#include "text.h"
#include "xchatc.h"
#include "modes.h"
#include "outbound.h"
#include "ignore.h"
#include "inbound.h"
#include "dcc.h"
#include "ctcp.h"
#include "server.h"
#include "network.h"
#include "plugin.h"
#include "url.h"
#include "history.h"
#include "servlist.h"
#include "notify.h"
#include "tree.h"
#include "numeric.h"

/* Extra addons */
/* OpenSSL */
#ifdef USE_OPENSSL
#include <openssl/ssl.h>		  /* SSL_() */
#include <openssl/err.h>		  /* ERR_() */
#include "ssl.h"
extern SSL_CTX *ctx;				  /* xchat.c */
/* local variables */
static struct session *g_sess = NULL;
#endif

/* SOCKS support */
#ifdef SOCKS
#include <socks.h>
#endif

#endif

