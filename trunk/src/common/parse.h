#ifndef RAGE_PARSE_H
#define RAGE_PARSE_H

#define MAKE4(ch0, ch1, ch2, ch3)       (guint32)(ch0 | (ch1 << 8) | (ch2 << 16) | (ch3 << 24))

void skip_white(char **buf);
char *split_opt(char **buf);
char *split_cmd(char **buf);
int split_int(char **buf);
void split_cmd_parv(char *buf,int *parc, char *parv[]);
char *paste_parv(char *buf, size_t buf_size, int first, int last, char *parv[]);

#endif /* RAGE_PARSE_H */
