#include "textenums.h"

#ifndef XCHAT_TEXT_H
#define XCHAT_TEXT_H

#define EMIT_SIGNAL(i, sess, a, b, c, d, e) text_emit(i, sess, a, b, c, d)
#define WORD_URL     1
#define WORD_NICK    2
#define WORD_CHANNEL 3
#define WORD_HOST    4
#define WORD_EMAIL   5
#define WORD_DIALOG  -1

struct text_event
{
	char *name;
	char **help;
	int num_args;
	char *def;
};

int text_word_check (struct server *serv, char *word);
void PrintText (rage_session *sess, char *text);
void PrintTextf (rage_session *sess, char *format, ...);
void log_close (rage_session *sess);
void log_open (rage_session *sess);
void load_text_events (void);
void pevent_save (char *fn);
int pevt_build_string (const char *input, char **output, int *max_arg);
int pevent_load (char *filename);
void pevent_make_pntevts (void);
void text_emit (int index, rage_session *sess, char *a, char *b, char *c, char *d);
int text_emit_by_name (char *name, rage_session *sess, char *a, char *b, char *c, char *d);
char *text_validate (char **text, size_t *len);
int get_stamp_str (char *fmt, time_t tim, char **ret);

void sound_play (const char *file);
void sound_play_event (int i);
void sound_beep (rage_session *);
void sound_load (void);
void sound_save (void);

#endif
