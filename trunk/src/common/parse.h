void skip_white(char **buf);
char *split_opt(char **buf);
char *split_cmd(char **buf);
int split_int(char **buf);
void split_cmd_parv(char *buf,int *parc, char *parv[]);
char *paste_parv(char *buf, size_t buf_size, int first, int last, char *parv[]);
	
