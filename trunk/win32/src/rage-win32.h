#ifdef WIN32
#ifndef _RAGE_WIN32_H
#define _RAGE_WIN32_H
/*
** rage-win32.h
** Windows specific includes and stuffs.
**
** You should not include this file directly, it is impliciately loaded
** on Windows in rage.h when WIN32 is defined. Do not include on other
** platforms or systems.
**
** (c) 2004 The Rage Development Team
*/

#ifndef NDEBUG
// turn off some security warnings while building in debug mode
#pragma warning(disable:4996)
#endif

// get windows stuff
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sys/timeb.h>
#include <process.h>
#include <io.h>

// set some configuration defines
// FIXME: This is ALL subject to change (and will do!)
#define LOCALEDIR "./locale"
#define ENABLE_NLS
#undef USE_GMODULE
#undef USE_PLUGIN
#define PACKAGE "rage"
#define VERSION "2.4.0"
#define XCHATLIBDIR "."
#define XCHATSHAREDIR "."
#define OLD_PERL
#ifndef USE_IPV6
#define socklen_t int
#endif
#if _INTEGRAL_MAX_BITS >= 64
#define stat _stati64
#define lseek _lseeki64
#define fstat _fstati64
#endif

// include specifics directly to windows
#define strcasecmp stricmp
#define strncasecmp strnicmp

// make sure server.c can find the identd hack
void identd_start (char *username);

#endif
#endif