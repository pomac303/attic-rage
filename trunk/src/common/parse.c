#include "rage.h"

void
skip_white(char **buf)
{
	while(**buf==' ')
		(*buf)++;
}

/* returns
 *  NULL : No option
 *  else : pointer to the option
 *  
 * side effects:
 *  buf now points at the end of the option
 *  buf is modified (\0 inserted)
 * 
 * TODO:
 *  Return an argument to an option eg
 *   --foo=6 should return "foo" and "6"
 */
char *
split_opt(char **buf)
{
	char *ret = NULL;

	skip_white(buf);

	if (**buf=='-')
	{
		(*buf)++;
		ret=(*buf);
		while(**buf && **buf!=' ')
			(*buf)++;
		
		if (**buf)
			*((*buf)++)='\0';
	}
	return ret;
}

/* Get the next word in a line
 *
 * Returns
 *  NULL : End of line
 *  other: Pointer to a word
 *
 * Side effects:
 *  updates *buf
 *  modified *buf
 * 
 * ' foo bar baz' => 'foo' 'bar baz'
 * ' "foo bar" baz' => 'foo bar' ' baz'
 */
char *
split_cmd(char **buf)
{
	char *ret = 0;

	skip_white(buf);

	if (**buf=='"') /* Quoted */
	{
		(*buf)++;
		ret=*buf;

		while(**buf && **buf!='"')
			(*buf)++;

		if (**buf)
		{
			**buf='\0';
			(*buf)++;
		}
	} else 
	{
		ret=*buf;

		while(**buf && **buf!=' ')
			(*buf)++;

		if (**buf)
		{
			**buf='\0';
			(*buf)++;
		}
	}
	return ret;
}

/* Split a command line up into parc,parv
 * using command line rules
 */
void
split_cmd_parv(char *buf,int *parc, char *parv[])
{
	int i=0;
	*parc=0;

	while(*buf) 
	{
		parv[(*parc)++]=split_cmd(&buf);
		if (*parc>(MAX_TOKENS-1))
		{
			parv[*parc]=buf;
			break;
		}
	}
	for(i=*parc;i<MAX_TOKENS;i++)
		parv[i]="";
}

/* Join a parv string and return the result */
char *
paste_parv(char *buf, size_t buf_size, int first, int last, char *parv[])
{
	char *lp = buf;
	int i;
	*lp='\0';

	for(i=first;i<last && lp<(buf+buf_size);i++)
		lp+=snprintf(lp,(gulong)buf_size-(lp-buf),"%s ",parv[i]);
	return buf;
}
		
