#ifndef RAGE_COMMONPLUGIN_H
#define RAGE_COMMONPLUGIN_H

#include "rage-plugin.h"

char *plugin_load (rage_session *sess, char *filename, char *arg);
void plugin_add (rage_session *sess, char *filename, void *handle, void *init_func, void *deinit_func, char *arg, int fake);
int plugin_kill (char *name, int by_filename);
void plugin_kill_all (void);
void plugin_auto_load (rage_session *sess);
int plugin_emit_command (rage_session *sess, char *name, char *cmd);
int plugin_emit_server (rage_session *sess, char *name, int parc, char *parv[]);
int plugin_emit_print (rage_session *sess, int parc, char *parv[]);
int plugin_emit_dummy_print (rage_session *sess, char *name);
GList* plugin_command_list(GList *tmp_list);
int plugin_show_help (rage_session *sess, char *cmd);
void setup_plugin_commands(void);

#endif
