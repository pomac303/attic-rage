/* X-Chat
 * Copyright (C) 1998 Peter Zelezny.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include "rage.h"
#include <assert.h>

static int
nick_cmp_az_ops (server *serv, struct User *user1, struct User *user2)
{
	unsigned int access1 = user1->access;
	unsigned int access2 = user2->access;
	int pos;

	if (access1 != access2)
	{
		for (pos = 0; pos < USERACCESS_SIZE; pos++)
		{
			if ((access1&(1<<pos)) && (access2&(1<<pos)))
				break;
			if ((access1&(1<<pos)) && !(access2&(1<<pos)))
				return -1;
			if (!(access1&(1<<pos)) && (access2&(1<<pos)))
				return 1;
		}
	}

	return serv->p_cmp (user1->nick, user2->nick);
}

static int
nick_cmp_alpha (struct User *user1, struct User *user2, server *serv)
{
	return serv->p_cmp (user1->nick, user2->nick);
}

static int
nick_cmp (struct User *user1, struct User *user2, server *serv)
{
	switch (prefs.userlist_sort)
	{
	case 0:
		return nick_cmp_az_ops (serv, user1, user2);
	case 1:
		return serv->p_cmp (user1->nick, user2->nick);
	case 2:
		return -1 * nick_cmp_az_ops (serv, user1, user2);
	case 3:
		return -1 * serv->p_cmp (user1->nick, user2->nick);
	default:
		return 1;
	}
}

/*
 insert name in appropriate place in linked list. Returns row number or:
  -1: duplicate
*/

static int
userlist_insertname (rage_session *sess, struct User *newuser)
{
	if (!sess->usertree)
	{
		sess->usertree = tree_new ((tree_cmp_func *)nick_cmp, sess->server);
		sess->usertree_alpha = tree_new ((tree_cmp_func *)nick_cmp_alpha, sess->server);
	}

	tree_insert (sess->usertree_alpha, newuser);
	return tree_insert (sess->usertree, newuser);
}

void
userlist_set_away (rage_session *sess, char *nick, unsigned int away)
{
	struct User *user;

	user = find_name (sess, nick);
	if (user)
	{
		if (user->away != away)
		{
			user->away = away;
			/* rehash GUI */
			fe_userlist_rehash (sess, user);
		}
	}
}

int
userlist_add_hostname (rage_session *sess, char *nick, char *hostname,
							  char *realname, char *servername, unsigned int away)
{
	struct User *user;

	user = find_name (sess, nick);
	if (user)
	{
		if (!user->hostname && hostname)
			user->hostname = strdup (hostname);
		if (!user->realname && realname)
			user->realname = strdup (realname);
		if (!user->servername && servername)
			user->servername = strdup (servername);

		if (prefs.showhostname_in_userlist || user->away != away)
		{
			user->away = away;
			fe_userlist_rehash (sess, user);
		}
		user->away = away;
		return 1;
	}
	return 0;
}

static int
free_user (struct User *user, gpointer data)
{
	if (user->realname)
		free (user->realname);
	if (user->hostname)
		free (user->hostname);
	if (user->servername)
		free (user->servername);
	free (user);

	return TRUE;
}

void
free_userlist (rage_session *sess)
{
	tree_foreach (sess->usertree, (tree_traverse_func *)free_user, NULL);
	tree_destroy (sess->usertree);
	tree_destroy (sess->usertree_alpha);

	sess->usertree = NULL;
	sess->usertree_alpha = NULL;

	sess->ops = 0;
	sess->hops = 0;
	sess->voices = 0;
	sess->total = 0;
}

void
clear_user_list (rage_session *sess)
{
	fe_userlist_clear (sess);
	free_userlist (sess);
	fe_userlist_numbers (sess);
}

static int
find_cmp (const char *name, struct User *user, server *serv)
{
	return serv->p_cmp ((char *)name, user->nick);
}

struct User *
find_name (rage_session *sess, char *name)
{
	int pos;

	if (sess->usertree_alpha)
		return tree_find (sess->usertree_alpha, name,
								(tree_cmp_func *)find_cmp, sess->server, &pos);

	return NULL;
}

struct User *
find_name_global (struct server *serv, char *name)
{
	struct User *user;
	rage_session *sess;
	GSList *list = sess_list;
	while (list)
	{
		sess = (rage_session *) list->data;
		if (sess->server == serv)
		{
			user = find_name (sess, name);
			if (user)
				return user;
		}
		list = list->next;
	}
	return 0;
}

int
find_ctarget (struct server *serv, char *channel, char *name)
{
	rage_session *sess;
	GSList *list = sess_list;
	while (list)
	{
		sess = (rage_session *) list->data;
		if (sess->server == serv && sess->me && sess->me->op)
		{
			if (find_name(sess, name))
			{
				strncpy(channel, sess->channel, CHANLEN);
				return 1;
			}
		}
		list = list->next;
	}
	return 0;
}

static void
update_counts (rage_session *sess, struct User *user, char prefix,
					int level, int offset)
{
	switch (prefix)
	{
	case '@':
		user->op = level;
		sess->ops += offset;
		break;
	case '%':
		user->hop = level;
		sess->hops += offset;
		break;
	case '+':
		user->voice = level;
		sess->voices += offset;
		break;
	}
}

void
ul_update_entry (rage_session *sess, char *name, char mode, char sign)
{
	int access;
	int offset = 0;
	int level;
	int pos;
	char prefix;
	struct User *user;

	user = find_name (sess, name);
	if (!user)
		return;

	/* remove from binary trees, before we loose track of it */
	tree_remove (sess->usertree, user, &pos);
	tree_remove (sess->usertree_alpha, user, &pos);

	/* which bit number is affected? */
	access = mode_access (sess->server, mode, &prefix);

	if (sign == '+')
	{
		level = TRUE;
		if (!(user->access & (1 << access)))
		{
			offset = 1;
			user->access |= (1 << access);
		}
	} else
	{
		level = FALSE;
		if (user->access & (1 << access))
		{
			offset = -1;
			user->access &= ~(1 << access);
		}
	}

	/* now what is this users highest prefix? e.g. @ for ops */
	user->prefix[0] = get_nick_prefix (sess->server, user->access);

	/* update the various counts using the CHANGED prefix only */
	update_counts (sess, user, prefix, level, offset);

	/* insert it back into its new place */
	tree_insert (sess->usertree_alpha, user);
	pos = tree_insert (sess->usertree, user);

	/* let GTK move it too */
	fe_userlist_move (sess, user, pos);
	fe_userlist_numbers (sess);
}

int
change_nick (rage_session *sess, char *oldname, char *newname)
{
	struct User *user = find_name (sess, oldname);
	int pos;

	if (user)
	{
		tree_remove (sess->usertree, user, &pos);
		tree_remove (sess->usertree_alpha, user, &pos);

		safe_strcpy (user->nick, newname, NICKLEN);

		tree_insert (sess->usertree_alpha, user);

		fe_userlist_move (sess, user, tree_insert (sess->usertree, user));
		fe_userlist_numbers (sess);

		return 1;
	}

	return 0;
}

int
sub_name (rage_session *sess, char *name)
{
	struct User *user;
	int pos;

	user = find_name (sess, name);
	if (!user)
		return FALSE;

	if (user->voice)
		sess->voices--;
	if (user->op)
		sess->ops--;
	if (user->hop)
		sess->hops--;
	sess->total--;
	fe_userlist_numbers (sess);
	fe_userlist_remove (sess, user);

	if (user == sess->me)
		sess->me = NULL;

	tree_remove (sess->usertree, user, &pos);
	tree_remove (sess->usertree_alpha, user, &pos);
	free_user (user, NULL);

	return TRUE;
}

struct User*
add_name (rage_session *sess, char *name, char *hostname)
{
	struct User *user;
	int row, prefix_chars;
	unsigned int acc;

	acc = nick_access (sess->server, name, &prefix_chars);

	notify_set_online (sess->server, name + prefix_chars);

	user = malloc (sizeof (struct User));
	memset (user, 0, sizeof (struct User));

	user->access = acc;

	/* assume first char is the highest level nick prefix */
	if (prefix_chars)
		user->prefix[0] = name[0];

	/* add it to our linked list */
	if (hostname)
		user->hostname = strdup (hostname);
	safe_strcpy (user->nick, name + prefix_chars, NICKLEN);
	/* is it me? */
	if (!sess->server->p_cmp (user->nick, sess->server->nick))
		user->me = TRUE;
	row = userlist_insertname (sess, user);

	/* duplicate? some broken servers trigger this */
	if (row == -1)
	{
		struct User *tmp = find_name(sess, user->nick);
		
		if (user->hostname)
			free (user->hostname);
		free (user);

		return tmp; /* needed for the MJOIN code */
	}

	sess->total++;

	/* most ircds don't support multiple modechars infront of the nickname
	 * for /NAMES - though they should. */
	while (prefix_chars)
	{
		update_counts (sess, user, name[0], TRUE, 1);
		name++;
		prefix_chars--;
	}

	if (user->me)
		sess->me = user;

	fe_userlist_insert (sess, user, row, FALSE);
	fe_userlist_numbers (sess);
	return user;
}

static int
rehash_cb (struct User *user, rage_session *sess)
{
	fe_userlist_rehash (sess, user);
	return TRUE;
}

void
userlist_rehash (rage_session *sess)
{
	tree_foreach (sess->usertree_alpha, (tree_traverse_func *)rehash_cb, sess);
}

static int
flat_cb (struct User *user, GSList **list)
{
	*list = g_slist_prepend (*list, user);
	return TRUE;
}

GSList *
userlist_flat_list (rage_session *sess)
{
	GSList *list = NULL;

	tree_foreach (sess->usertree_alpha, (tree_traverse_func *)flat_cb, &list);
	return g_slist_reverse (list);
}

static int
double_cb (struct User *user, GList **list)
{
	*list = g_list_prepend(*list, user);
	return TRUE;
}

GList *
userlist_double_list(rage_session *sess)
{
	GList *list = NULL;

	tree_foreach (sess->usertree_alpha, (tree_traverse_func *)double_cb, &list);
	return list;
}

/** Add a user to a session */
void rage_add_user(rage_session *sess, struct User *user)
{
	struct Membership *member;

	member = (struct Membership *)malloc(sizeof(struct Membership));
	member->prev_user = NULL;
	member->next_user = user->members;
	if (member->next_user)
		member->next_user->prev_user = member;
	user->members = member;

	member->prev_session = NULL;
	member->next_session = sess->members;
	if (member->next_session)
		member->next_session->prev_session = member;
	sess->members = member;

	member->op = 0;
	member->hop = 0;
	member->voice = 0;
	member->away = 0;
}

/** Remove a user from a channel */
void rage_del_user(rage_session *sess, struct User *user)
{
	struct Membership *member;
	/* TODO: we can step down sess or user, which ever is faster */
	for(member = sess->members; member!=NULL; member=member->next_session){
		if (member->user == user) {
			break;
		}
	}
	/* If this fails then we're removing an item that doesn't exist */
	assert (member->user == user);

	/* Now unlink it from the sparse matrix */
	if (member->prev_user)
		member->prev_user->next_user = member->next_user;
	if (member->next_user)
		member->next_user->prev_user = member->prev_user;
	if (member->prev_session)
		member->prev_session->next_session = member->next_session;
	if (member->next_session)
		member->next_session->prev_session = member->prev_session;

	/* Now, was this the last session for this user? */
	if (member->user->members != NULL) {
		free_user(member->user, NULL);
	}
}

void foreach_membership_user(rage_session *sess, tree_traverse_func *func, 
		void *data) {
	struct Membership *member;
	for(member = sess->members; member!=NULL; member=member->next_user) {
		func(member,data);
	}
}

void foreach_membership_sess(struct User *user, tree_traverse_func *func,
		void *data) {
	struct Membership *member;
	for(member = user->members; member!=NULL; member=member->next_session) {
		func(member,data);
	}
}

/* Some examples */
#if 0

void nick_change_cb(struct Membership *mem, void *nick)
{
	send_message_to_session(mem->session,"Nick changed from %s to %s",
			mem->user->nick,nick);
}

void nick_change(struct User *user,char *newnick)
{
	foreach_membership_sess(user, nick_change_cb, newnick);
	strlcpy(user->nick,sizeof(user->nick),newnick);
}

void nick_quit_cb(struct Membership *mem, void *msg)
{
	send_message_to_session(mem->session,"User quit %s",(char*)msg);
}

void nick_quit(struct User *user, void *msg)
{
	foreach_membership_sess(user, nick_quit_cb, msg);
	strlcpy(user->nick,sizeof(user->nick),msg);
}

typedef enum {SPLIT, JOIN, NONE} stack_type_t;

void flush_stack(struct stack *stk)
{
	/* output the stack */
	if (stk->which==JOIN) {
		output_to_a_session_coz_Im_bored(stk->session,
				"JOIN",stk->queue);
	} else {
		output_to_a_session_coz_Im_bored(stk->session,
				"QUIT",stk->queue);
	}
}

void append_to_stack(struct stack *stk, 
		stack_type_t which,
		struct Membership *mem,
		char *msg)
{
	/* If it's a different join/part then flush() */
	if (stk->which != which ||
		 	( (msg == NULL && stk->msg != NULL)
			||(msg != NULL && stk->msg == NULL)  
			||(strcmp(msg,stk->msg)==0) )
			) 
			|| stk->session != mem->session) {
		flush_stack(stk);
		stk->which = NONE;
	}

	if (stk->which == NONE) {
		stk->which = which;
		if (stk->msg)
			free(stk->msg);
		if (msg)
			stk->msg = strdup(msg);
		else
			stk->msg = NULL;

		stk->session = mem->session;
	}

	push_stack(stk->queue,mem->user->nick);
	
}


void nick_splitting_cb(struct Membership *mem, void *msg)
{
	append_to_stack(mem->sess->stack,SPLIT,mem,msg);
}

void nick_splitting(struct User *user, void *msg)
{
	foreach_membership_sess(user, nick_splitting_cb, msg);
}

void nick_joining_cb(struct Membership *mem, void *msg)
{
	append_to_stack(mem->sess->stack,JOIN,mem,NULL);
}

void nick_joining(struct User *user)
{
	foreach_membership_sess(user, nick_joining_cb, NULL);
}

#endif
