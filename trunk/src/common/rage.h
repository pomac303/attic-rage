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
#define _LARGEFILE_SOURCE
#define _GNU_SOURCE

/* Common make4 */
#define MAKE4(ch0, ch1, ch2, ch3)       (guint32)(ch0 | (ch1 << 8) | (ch2 << 16) | (ch3 << 24))
#define MAKE4UPPER(ch0, ch1, ch2, ch3)  (guint32)(toupper(ch0) | (toupper(ch1) << 8) | \
		(toupper(ch2) << 16) | (toupper(ch3) << 24))

/* For ctcp parsing */
#define C_VERSION	MAKE4('V','E','R','S')
#define C_ACTION	MAKE4('A','C','T','I')
#define C_SOUND		MAKE4('S','O','U','N')

/* For dcc parsing */
#define D_DCC		MAKE4('D','C','C',' ')
#define D_CHAT		MAKE4('C','H','A','T')
#define D_RESUME	MAKE4('R','E','S','U')
#define D_ACCEPT	MAKE4('A','C','C','E')
#define D_SEND		MAKE4('S','E','N','D')
#define D_PSEND		MAKE4('P','S','E','N')
#define D_CLOSE		MAKE4('C','L','O','S')
#define D_LIST		MAKE4('L','I','S','T')
#define D_HELP		MAKE4('H','E','L','P')
#define D_GET		MAKE4('G','E','T','\0')

/* For cmd_gui in src/common/outbound.c */
#define G_HIDE		MAKE4('H','I','D','E')
#define G_SHOW		MAKE4('S','H','O','W')
#define G_FOCUS		MAKE4('F','O','C','U')
#define G_FLASH		MAKE4('F','L','A','S')
#define G_COLOR		MAKE4('C','O','L','O')
#define G_ICONIFY	MAKE4('I','C','O','N')

/* For ignore command parsing in src/common/outbound.c */
#define I_UNIGNORE	MAKE4('U','N','I','G')
#define I_ALL		MAKE4('A','L','L','\0')
#define I_PRIV		MAKE4('P','R','I','V')
#define I_NOTI		MAKE4('N','O','T','I')
#define I_CHAN		MAKE4('C','H','A','N')
#define I_CTCP		MAKE4('C','T','C','P')
#define I_INVI		MAKE4('I','N','V','I')
#define I_QUIET		MAKE4('Q','U','I','E')
#define I_NOSAVE	MAKE4('N','O','S','A')
#define I_DCC		MAKE4('D','C','C','\0')

#include "config.h"

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
#include <sys/utsname.h>
#include <dirent.h>
#include <regex.h>
#include <sys/time.h>
#endif

/* OS X */
/* TODO: Let the configure script handle this */
#if defined(__APPLE__) && defined(__MACH__)
#include <sys/sysctl.h>
#endif

/* Require glib */
#include <glib/gslist.h>
#include <glib/glist.h>
#include <glib/gutils.h>
#include <glib/giochannel.h>
#include <glib/gstrfuncs.h>
#include <glib/ghash.h>
#include <glib/gcompletion.h>
#ifndef HAVE_SNPRINTF
#define snprintf g_snprintf
#endif

/* Platform specifics */
#ifdef WIN32
#include "rage-win32.h"
#endif

/* 64-bit stuff */
#if defined(__linux__)
	#include <byteswap.h>
	#if __BYTE_ORDER == __LITTLE_ENDIAN
	#define htonll(x)	bswap_64(x)
	#define ntohll(x)	bswap_64(x)
	#else
	#define htonll(x)	(x)
	#define ntohll(x)	(x)
	#endif
#elif defined(__APPLE__) && defined(__MACH__)
	#if __BYTE_ORDER == __BIG_ENDIAN
	#define ntohll(x)	(x)
	#define htonll(x)	(x)
	#else
	/* Can't hurt */
	#define ntohll(x) (((int64_t)(ntohl((int)((x << 32) >> 32))) << 32) | (unsigned int)ntohl(((int)(x >> 32))))
	#define htonll(x) ntohll(x)
	#endif
#elif defined(WIN32)
	#define ntohll(x) (((__int64)(ntohl((int)((x << 32) >> 32))) << 32) | (unsigned int)ntohl(((int)(x >> 32))))
	#define htonll(x) ntohll(x)
#else
	#error No 64-bit stuff defined for your platform
#endif

extern char rage_svn_version[];

/* XXX: configure */
#define NDEBUG

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
#else
#undef strdup
#define malloc g_malloc
#define realloc g_realloc
#define free g_free
#define strdup g_strdup
#endif

#include "dict.h"
#include "rage_data.h"

/* Include Internet compatibility stuff */
#define WANTSOCKET
#define WANTARPA
#define WANTDNS
#include "inet.h"

#define RAGE_INTERNAL

/* Include common includes */
/* Bletch! */
#include "rage-plugin.h"
#include "cfgfiles.h"
#include "proto-irc.h"
#include "util.h"
#include "fe.h"
#include "text.h"
#include "ragec.h"
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
#include "parse.h"

/* Extra addons */
/* OpenSSL */
#ifdef USE_OPENSSL
#include <openssl/ssl.h>		  /* SSL_() */
#include <openssl/err.h>		  /* ERR_() */
#include "ssl.h"
extern SSL_CTX *ctx;				  /* rage.c */
/* local variables */
#endif

/* SOCKS support */
#ifdef SOCKS
#include <socks.h>
#endif

#endif

