// gateway.cpp

#pragma once

using namespace System;

namespace rage
{
	namespace gateway
	{
		public __gc class WindowsGUI
		{
			// Define the gateway functions
			//
			/* Methods that this must provide a gateway for:
			** void fe_message(char *, int);
			** void xchat_exec(char *);
			** void fe_dcc_update(struct DCC *);
			** void fe_dcc_remove(struct DCC *);
			** void fe_input_remove(int);
			** int fe_input_add(int,int,void *,void *);
			** void fe_dcc_add(struct DCC *);
			** int fe_dcc_open_send_win(int);
			** int plugin_emit_print(struct session *,int,char * *);
			** int fe_dcc_open_chat_win(int);
			** int fe_dcc_open_recv_win(int);
			** void fe_confirm(char const *,void (*)(void *),void (*)(void *),void *);
			** void fe_ignore_update(int);
			** void fe_set_title(struct session *);
			** void fe_set_nonchannel(struct session *,int);
			** void fe_clear_channel(struct session *);
			** void fe_timeout_remove(int);
			** void fe_set_topic(struct session *,char *);
			** void fe_set_hilight(struct session *);
			** void fe_set_nick(struct server *,char *);
			** void fe_set_channel(struct session *);
			** void fe_set_lag(struct server *,int);
			** void fe_set_away(struct server *);
			** void fe_add_ban_list(struct session *,char *,char *,char *);
			** int fe_is_banwindow(struct session *);
			** void fe_update_channel_limit(struct session *);
			** void fe_update_mode_buttons(struct session *,char,char);
			** void fe_update_channel_key(struct session *);
			** int fe_timeout_add(int,void *,void *);
			** void fe_notify_update(char *);
			** void fe_buttons_update(struct session *);
			** void fe_dlgbuttons_update(struct session *);
			** void fe_text_clear(struct session *);
			** void fe_close_window(struct session *);
			** void fe_dcc_send_filereq(struct session *,char *,int,int);
			** void fe_get_int(char *,int,void *,void *);
			** void fe_get_str(char *,char *,void *,void *);
			** void fe_ctrl_gui(struct session *,int,int);
			** int plugin_show_help(struct session *,char *);
			** void xchat_exit(void);
			** void fe_lastlog(struct session *,struct session *,char *);
			** int plugin_emit_command(struct session *,char *,char *);
			** int plugin_emit_server(struct session *,char *,int,char * *);
			** void fe_ban_list_end(struct session *);
			** void fe_chan_list_end(struct server *);
			** void fe_add_chan_list(struct server *,char *,char *,char *);
			** int fe_is_chanwindow(struct server *);
			** void fe_add_rawlog(struct server *,char *,int,int);
			** void fe_set_throttle(struct server *);
			** void fe_progressbar_end(struct server *);
			** void fe_progressbar_start(struct session *);
			** void fe_print_text(struct session *,char *);
			** void fe_beep(void);
			** void fe_url_add(char const *);
			** void fe_userlist_rehash(struct session *,struct User *);
			** void fe_userlist_numbers(struct session *);
			** void fe_userlist_clear(struct session *);
			** void fe_userlist_move(struct session *,struct User *,int);
			** int fe_userlist_remove(struct session *,struct User *);
			** void fe_userlist_insert(struct session *,struct User *,int,int);
			** struct _GList *plugin_command_list(struct _GList *);
			** int plugin_emit_dummy_print(struct session *,char *);
			** void fe_new_window(struct session *,int);
			** void fe_new_server(struct server *);
			** void fe_session_callback(struct session *);
			** void fe_server_callback(struct server *);
			*/
		public: 
			void test()
			{
				int i; i = 0;
			}
		};
	}
}
