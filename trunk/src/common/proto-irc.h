#ifndef XCHAT_PROTO_H
#define XCHAT_PROTO_H

void proto_fill_her_up (server *serv);
/* Splits a line up from the server */
void irc_split(server *serv,char *buf,int *parc,char *parv[]);
void split_cmd_parv(char *buf,int *parc,char *parv[]);
void setup_parser(void);


#endif
