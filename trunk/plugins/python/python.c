/*
* Copyright (c) 2002-2003  Gustavo Niemeyer <niemeyer@conectiva.com>
*
* Rage Python Plugin Interface
*
* Derived from the XChat Python interface
*
* Rage Python Plugin Interface is free software; you can redistribute
* it and/or modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2 of the
* License, or (at your option) any later version.
*
* pybot is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this file; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Thread support
 * ==============
 *
 * The python interpreter has a global interpreter lock. Any thread
 * executing must acquire it before working with data accessible from
 * python code. Here we must also care about rage not being
 * thread-safe. We do this by using an rage lock, which protects
 * rage instructions from being executed out of time (when this
 * plugin is not "active").
 *
 * When rage calls python code:
 *   - Change the current_plugin for the executing plugin;
 *   - Release rage lock
 *   - Acquire the global interpreter lock
 *   - Make the python call
 *   - Release the global interpreter lock
 *   - Acquire rage lock
 *
 * When python code calls rage:
 *   - Release the global interpreter lock
 *   - Acquire rage lock
 *   - Restore context, if necessary
 *   - Make the rage call
 *   - Release rage lock
 *   - Acquire the global interpreter lock
 *
 * Inside a timer, so that individual threads have a chance to run:
 *   - Release rage lock
 *   - Go ahead threads. Have a nice time!
 *   - Acquire rage lock
 *
 */

#include "Python.h"
#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

#include "rage-plugin.h"
#include "structmember.h"
#include "pythread.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION "0.2"

#ifdef WIN32
#undef WITH_THREAD /* Thread support locks up rage on Win32. */
#endif

#define NONE 0
#define ALLOW_THREADS 1
#define RESTORE_CONTEXT 2

#ifdef WITH_THREAD
#define ACQUIRE_RAGE_LOCK() PyThread_acquire_lock(rage_lock, 1)
#define RELEASE_RAGE_LOCK() PyThread_release_lock(rage_lock)
#define BEGIN_RAGE_CALLS(x) \
	do { \
		PyObject *calls_plugin = NULL; \
		PyThreadState *calls_thread; \
		if ((x) & RESTORE_CONTEXT) \
			calls_plugin = Plugin_GetCurrent(); \
		calls_thread = PyEval_SaveThread(); \
		ACQUIRE_RAGE_LOCK(); \
		if (!((x) & ALLOW_THREADS)) { \
			PyEval_RestoreThread(calls_thread); \
			calls_thread = NULL; \
		} \
		if (calls_plugin) \
			rage_set_context(ph, \
				Plugin_GetContext(calls_plugin)); \
		while (0)
#define END_RAGE_CALLS() \
		RELEASE_RAGE_LOCK(); \
		if (calls_thread) \
			PyEval_RestoreThread(calls_thread); \
	} while(0)
#else
#define ACQUIRE_RAGE_LOCK()
#define RELEASE_RAGE_LOCK()
#define BEGIN_RAGE_CALLS(x)
#define END_RAGE_CALLS()
#endif

#define BEGIN_PLUGIN(plg) \
	do { \
	rage_context *begin_plugin_ctx = rage_get_context(ph); \
	RELEASE_RAGE_LOCK(); \
	Plugin_AcquireThread(plg); \
	Plugin_SetContext(plg, begin_plugin_ctx); \
	} while (0)
#define END_PLUGIN(plg) \
	do { \
	Plugin_ReleaseThread(plg); \
	ACQUIRE_RAGE_LOCK(); \
	} while (0)

#define Plugin_Swap(x) \
	PyThreadState_Swap(((PluginObject *)(x))->tstate)
#define Plugin_AcquireThread(x) \
	PyEval_AcquireThread(((PluginObject *)(x))->tstate)
#define Plugin_ReleaseThread(x) \
	Util_ReleaseThread(((PluginObject *)(x))->tstate)
#define Plugin_GetFilename(x) \
	(((PluginObject *)(x))->filename)
#define Plugin_GetName(x) \
	(((PluginObject *)(x))->name)
#define Plugin_GetVersion(x) \
	(((PluginObject *)(x))->version)
#define Plugin_GetDesc(x) \
	(((PluginObject *)(x))->description)
#define Plugin_GetHooks(x) \
	(((PluginObject *)(x))->hooks)
#define Plugin_GetContext(x) \
	(((PluginObject *)(x))->context)
#define Plugin_SetFilename(x, y) \
	((PluginObject *)(x))->filename = (y);
#define Plugin_SetName(x, y) \
	((PluginObject *)(x))->name = (y);
#define Plugin_SetVersion(x, y) \
	((PluginObject *)(x))->version = (y);
#define Plugin_SetDescription(x, y) \
	((PluginObject *)(x))->description = (y);
#define Plugin_SetHooks(x, y) \
	((PluginObject *)(x))->hooks = (y);
#define Plugin_SetContext(x, y) \
	((PluginObject *)(x))->context = (y);

#define HOOK_RAGE  1
#define HOOK_UNLOAD 2

/* ===================================================================== */
/* Object definitions */

typedef struct {
	PyObject_HEAD
	int softspace; /* We need it for print support. */
} RageOutObject;

typedef struct {
	PyObject_HEAD
	rage_context *context;
} ContextObject;

typedef struct {
	PyObject_HEAD
	const char *listname;
	PyObject *dict;
} ListItemObject;

typedef struct {
	PyObject_HEAD
	char *name;
	char *version;
	char *filename;
	char *description;
	GSList *hooks;
	PyThreadState *tstate;
	rage_context *context;
	void *gui;
} PluginObject;

typedef struct {
	int type;
	PyObject *plugin;
	PyObject *callback;
	PyObject *userdata;
	void *data; /* A handle, when type == HOOK_RAGE */
} Hook;


/* ===================================================================== */
/* Function declarations */

static PyObject *Util_BuildList(int parc, char *parv[]);
static void Util_Autoload(void);
static char *Util_Expand(char *filename);

static int Callback_Command(int parc, char *parv[], void *userdata);
static int Callback_Print(int parc, char *parv[], void *userdata);
static int Callback_Timer(void *userdata);
static int Callback_ThreadTimer(void *userdata);

static PyObject *RageOut_New(void);
static PyObject *RageOut_write(PyObject *self, PyObject *args);
static void RageOut_dealloc(PyObject *self);

static void Context_dealloc(PyObject *self);
static PyObject *Context_set(ContextObject *self, PyObject *args);
static PyObject *Context_command(ContextObject *self, PyObject *args);
static PyObject *Context_prnt(ContextObject *self, PyObject *args);
static PyObject *Context_get_info(ContextObject *self, PyObject *args);
static PyObject *Context_get_list(ContextObject *self, PyObject *args);
static PyObject *Context_FromContext(rage_context *context);
static PyObject *Context_FromServerAndChannel(char *server, char *channel);

static PyObject *Plugin_New(char *filename, PyMethodDef *rage_methods,
			    PyObject *xcoobj);
static PyObject *Plugin_GetCurrent(void);
static PluginObject *Plugin_ByString(char *str);
static Hook *Plugin_AddHook(int type, PyObject *plugin, PyObject *callback,
			    PyObject *userdata, void *data);
static void Plugin_RemoveHook(PyObject *plugin, Hook *hook);
static void Plugin_RemoveAllHooks(PyObject *plugin);

static PyObject *Module_rage_command(PyObject *self, PyObject *args);
static PyObject *Module_rage_prnt(PyObject *self, PyObject *args);
static PyObject *Module_rage_get_context(PyObject *self, PyObject *args);
static PyObject *Module_rage_find_context(PyObject *self, PyObject *args,
					   PyObject *kwargs);
static PyObject *Module_rage_get_info(PyObject *self, PyObject *args);
static PyObject *Module_rage_hook_command(PyObject *self, PyObject *args,
					   PyObject *kwargs);
static PyObject *Module_rage_hook_server(PyObject *self, PyObject *args,
					  PyObject *kwargs);
static PyObject *Module_rage_hook_print(PyObject *self, PyObject *args,
					 PyObject *kwargs);
static PyObject *Module_rage_hook_timer(PyObject *self, PyObject *args,
					 PyObject *kwargs);
static PyObject *Module_rage_unhook(PyObject *self, PyObject *args);
static PyObject *Module_rage_get_info(PyObject *self, PyObject *args);
static PyObject *Module_rage_get_list(PyObject *self, PyObject *args);
static PyObject *Module_rage_get_lists(PyObject *self, PyObject *args);
static PyObject *Module_rage_nickcmp(PyObject *self, PyObject *args);

static void IInterp_Exec(char *command);
static int IInterp_Cmd(int parc, char *word[], void *userdata);

static void Command_PyList(void);
static void Command_PyLoad(char *filename);
static void Command_PyUnload(char *name);
static void Command_PyReload(char *name);
static void Command_PyAbout(void);
static int Command_Py(int parc, char *parv[], void *userdata);

/* ===================================================================== */
/* Static declarations and definitions */

staticforward PyTypeObject Plugin_Type;
staticforward PyTypeObject RageOut_Type;
staticforward PyTypeObject Context_Type;
staticforward PyTypeObject ListItem_Type;

static PyThreadState *main_tstate = NULL;
static void *thread_timer = NULL;

static rage_plugin *ph;
static GSList *plugin_list = NULL;

static PyObject *interp_plugin = NULL;
static PyObject *rageout = NULL;

#ifdef WITH_THREAD
static PyThread_type_lock rage_lock = NULL;
#endif

static char *usage = "\
Usage: /PY LOAD   <filename>\n\
           UNLOAD <filename|name>\n\
           RELOAD <filename|name>\n\
           LIST\n\
           EXEC <command>\n\
           CONSOLE\n\
           ABOUT\n\
\n";

static char *about = "\
\n\
X-Chat Python Interface " VERSION "\n\
\n\
Copyright (c) 2002-2003  Gustavo Niemeyer <niemeyer@conectiva.com>\n\
\n";


/* ===================================================================== */
/* Utility functions */

static PyObject *
Util_BuildList(int parc, char *parv[])
{
	PyObject *list;
	int i;
	list = PyList_New(parc);
	if (list == NULL) {
                PyErr_Print();
		return NULL;
	}
	for (i = 0; i != parc; i++) {
		PyObject *o = PyString_FromString(parv[i]);
		if (o == NULL) {
			Py_DECREF(list);
			PyErr_Print();
			return NULL;
		}
		PyList_SetItem(list, i, o);
	}
	return list;
}

static void
Util_Autoload(void)
{
	char oldcwd[PATH_MAX];
	const char *dir_name;
	struct dirent *ent;
	DIR *dir;
	if (getcwd(oldcwd, PATH_MAX) == NULL)
		return;
	/* we need local filesystem encoding for chdir, opendir etc */
	dir_name = rage_get_info(ph, "ragedirfs");
	/* fallback for pre-2.0.9 rage */
	if (!dir_name)
		dir_name = rage_get_info(ph, "ragedir");
	if (chdir(dir_name) != 0)
		return;
	dir = opendir(".");
	if (dir == NULL)
		return;
	while ((ent = readdir(dir))) {
		int len = strlen(ent->d_name);
		if (len > 3 && strcmp(".py", ent->d_name+len-3) == 0)
			Command_PyLoad(ent->d_name);
	}
	closedir(dir);
	chdir(oldcwd);
}

static char *
Util_Expand(char *filename)
{
	char *expanded;

	/* Check if this is an absolute path. */
	if (g_path_is_absolute(filename)) {
		if (g_file_test(filename, G_FILE_TEST_EXISTS))
			return g_strdup(filename);
		else
			return NULL;
	}

	/* Check if it starts with ~/ and expand the home if positive. */
	if (*filename == '~' && *(filename+1) == '/') {
		expanded = g_build_filename(g_get_home_dir(),
					    filename+2, NULL);
		if (g_file_test(expanded, G_FILE_TEST_EXISTS))
			return expanded;
		else {
			g_free(expanded);
			return NULL;
		}
	}

	/* Check if it's in the current directory. */
	expanded = g_build_filename(g_get_current_dir(),
				    filename, NULL);
	if (g_file_test(expanded, G_FILE_TEST_EXISTS))
		return expanded;
	g_free(expanded);

	/* Check if ~/.rage/<filename> exists. */
	expanded = g_build_filename(rage_get_info(ph, "ragedir"),
				    filename, NULL);
	if (g_file_test(expanded, G_FILE_TEST_EXISTS))
		return expanded;
	g_free(expanded);

	return NULL;
}

/* Similar to PyEval_ReleaseThread, but accepts NULL thread states. */
static void
Util_ReleaseThread(PyThreadState *tstate)
{
	PyThreadState *old_tstate;
	if (tstate == NULL)
		Py_FatalError("PyEval_ReleaseThread: NULL thread state");
	old_tstate = PyThreadState_Swap(NULL);
	if (old_tstate != tstate && old_tstate != NULL)
		Py_FatalError("PyEval_ReleaseThread: wrong thread state");
	PyEval_ReleaseLock();
}

/* ===================================================================== */
/* Hookable functions. These are the entry points to python code, besides
 * the load function, and the hooks for interactive interpreter. */

static int
Callback_Command(int parc,char *parv[], void *userdata)
{
	Hook *hook = (Hook *) userdata;
	PyObject *retobj;
	PyObject *word_list;
	int ret = 0;

	BEGIN_PLUGIN(hook->plugin);

	word_list = Util_BuildList(parc,parv);
	if (word_list == NULL) {
		END_PLUGIN(hook->plugin);
		return 0;
	}
	retobj = PyObject_CallFunction(hook->callback, "(OO)", word_list,
				       hook->userdata);
	Py_DECREF(word_list);

	if (retobj == Py_None) {
		ret = RAGE_EAT_NONE;
		Py_DECREF(retobj);
	} else if (retobj) {
		ret = PyInt_AsLong(retobj);
		Py_DECREF(retobj);
	} else {
		PyErr_Print();
	}

	END_PLUGIN(hook->plugin);

	return ret;
}

/* No Callback_Server() here. We use Callback_Command() as well. */

static int
Callback_Print(int parc, char *parv[], void *userdata)
{
	Hook *hook = (Hook *) userdata;
	PyObject *retobj;
	PyObject *word_list;
	int ret = 0;

	BEGIN_PLUGIN(hook->plugin);

	word_list = Util_BuildList(parc,parv);
	if (word_list == NULL) {
		END_PLUGIN(hook->plugin);
		return 0;
	}

	retobj = PyObject_CallFunction(hook->callback, "(OO)", word_list,
				       hook->userdata);
	Py_DECREF(word_list);

	if (retobj == Py_None) {
		ret = RAGE_EAT_NONE;
		Py_DECREF(retobj);
	} else if (retobj) {
		ret = PyInt_AsLong(retobj);
		Py_DECREF(retobj);
	} else {
		PyErr_Print();
	}

	END_PLUGIN(hook->plugin);

	return ret;
}

static int
Callback_Timer(void *userdata)
{
	Hook *hook = (Hook *) userdata;
	PyObject *retobj;
	int ret = 0;
	PyObject *plugin;

	plugin = hook->plugin;

	BEGIN_PLUGIN(hook->plugin);

	retobj = PyObject_CallFunction(hook->callback, "(O)", hook->userdata);

	if (retobj) {
		ret = PyObject_IsTrue(retobj);
		Py_DECREF(retobj);
	} else {
		PyErr_Print();
	}

	/* Returning 0 for this callback unhooks itself. */
	if (ret == 0)
		Plugin_RemoveHook(hook->plugin, hook);

	END_PLUGIN(plugin);

	return ret;
}

#ifdef WITH_THREAD
static int
Callback_ThreadTimer(void *userdata)
{
	RELEASE_RAGE_LOCK();
	usleep(1);
	ACQUIRE_RAGE_LOCK();
	return 1;
}
#endif

/* ===================================================================== */
/* RageOut object */

/* We keep this information global, so we can reset it when the
 * deinit function is called. */
/* XXX This should be somehow bound to the printing context. */
static char *rageout_buffer = NULL;
static int rageout_buffer_size = 0;
static int rageout_buffer_pos = 0;

static PyObject *
RageOut_New(void)
{
	RageOutObject *xcoobj;
	xcoobj = PyObject_New(RageOutObject, &RageOut_Type);
	if (xcoobj != NULL)
		xcoobj->softspace = 0;
	return (PyObject *) xcoobj;
}

static void
RageOut_dealloc(PyObject *self)
{
	self->ob_type->tp_free((PyObject *)self);
}

/* This is a little bit complex because we have to buffer data
 * until a \n is received, since rage breaks the line automatically.
 * We also crop the last \n for this reason. */
static PyObject *
RageOut_write(PyObject *self, PyObject *args)
{
	int new_buffer_pos, data_size, print_limit, add_space;
	char *data, *pos;
	if (!PyArg_ParseTuple(args, "s#:write", &data, &data_size))
		return NULL;
	if (!data_size) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	BEGIN_RAGE_CALLS(RESTORE_CONTEXT|ALLOW_THREADS);
	if (((RageOutObject *)self)->softspace) {
		add_space = 1;
		((RageOutObject *)self)->softspace = 0;
	} else {
		add_space = 0;
	}
	if (rageout_buffer_size-rageout_buffer_pos < data_size+add_space) {
		char *new_buffer;
		/* This buffer grows whenever needed, and does not
		 * shrink. If we ever implement unloading of the
		 * python interface, we must find some way to free
		 * this buffer as well. */
		rageout_buffer_size += data_size*2+16;
		new_buffer = g_realloc(rageout_buffer, rageout_buffer_size);
		if (new_buffer == NULL) {
			rage_print(ph, "Not enough memory to print");
			/* The system is out of resources. Let's help. */
			g_free(rageout_buffer);
			rageout_buffer = NULL;
			rageout_buffer_size = 0;
			rageout_buffer_pos = 0;
			/* Return something valid, since we have
			 * already warned the user, and he probably
			 * won't be able to notice this exception. */
			goto exit;
		}
		rageout_buffer = new_buffer;
	}
	memcpy(rageout_buffer+rageout_buffer_pos, data, data_size);
	print_limit = new_buffer_pos = rageout_buffer_pos+data_size;
	pos = rageout_buffer+print_limit;
	if (add_space && *(pos-1) != '\n') {
		*pos = ' ';
		*(pos+1) = 0;
		new_buffer_pos++;
	}
	while (*pos != '\n' && print_limit > rageout_buffer_pos) {
		pos--;
		print_limit--;
	}
	if (*pos == '\n') {
		/* Crop it, inserting the string limiter there. */
		*pos = 0;
		rage_print(ph, rageout_buffer);
		if (print_limit < new_buffer_pos) {
			/* There's still data to be printed. */
			print_limit += 1; /* Include the limiter. */
			rageout_buffer_pos = new_buffer_pos-print_limit;
			memmove(rageout_buffer, rageout_buffer+print_limit,
				rageout_buffer_pos);
		} else {
			rageout_buffer_pos = 0;
		}
	} else {
		rageout_buffer_pos = new_buffer_pos;
	}

exit:
	END_RAGE_CALLS();
	Py_INCREF(Py_None);
	return Py_None;
}

#define OFF(x) offsetof(RageOutObject, x)

static PyMemberDef RageOut_members[] = {
	{"softspace", T_INT, OFF(softspace), 0},
	{0}
};

static PyMethodDef RageOut_methods[] = {
	{"write", RageOut_write, METH_VARARGS},
	{NULL, NULL}
};

statichere PyTypeObject RageOut_Type = {
	PyObject_HEAD_INIT(NULL)
	0,			/*ob_size*/
	"rage.RageOut",	/*tp_name*/
	sizeof(RageOutObject),	/*tp_basicsize*/
	0,			/*tp_itemsize*/
	RageOut_dealloc,	/*tp_dealloc*/
	0,			/*tp_print*/
	0,			/*tp_getattr*/
	0,			/*tp_setattr*/
	0,			/*tp_compare*/
	0,			/*tp_repr*/
	0,			/*tp_as_number*/
	0,			/*tp_as_sequence*/
	0,			/*tp_as_mapping*/
	0,			/*tp_hash*/
        0,                      /*tp_call*/
        0,                      /*tp_str*/
        PyObject_GenericGetAttr,/*tp_getattro*/
        PyObject_GenericSetAttr,/*tp_setattro*/
        0,                      /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT,     /*tp_flags*/
        0,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        RageOut_methods,       /*tp_methods*/
        RageOut_members,       /*tp_members*/
        0,                      /*tp_getset*/
        0,                      /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        0,                      /*tp_init*/
        PyType_GenericAlloc,    /*tp_alloc*/
        PyType_GenericNew,      /*tp_new*/
      	_PyObject_Del,          /*tp_free*/
        0,                      /*tp_is_gc*/
};


/* ===================================================================== */
/* Context object */

static void
Context_dealloc(PyObject *self)
{
	self->ob_type->tp_free((PyObject *)self);
}

static PyObject *
Context_set(ContextObject *self, PyObject *args)
{
	PyObject *plugin = Plugin_GetCurrent();
	Plugin_SetContext(plugin, self->context);
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
Context_command(ContextObject *self, PyObject *args)
{
	char *text;
	if (!PyArg_ParseTuple(args, "s:command", &text))
		return NULL;
	BEGIN_RAGE_CALLS(ALLOW_THREADS);
	rage_set_context(ph, self->context);
	rage_command(ph, text);
	END_RAGE_CALLS();
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
Context_prnt(ContextObject *self, PyObject *args)
{
	char *text;
	if (!PyArg_ParseTuple(args, "s:prnt", &text))
		return NULL;
	BEGIN_RAGE_CALLS(ALLOW_THREADS);
	rage_set_context(ph, self->context);
	rage_print(ph, text);
	END_RAGE_CALLS();
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
Context_emit_print(ContextObject *self, PyObject *args)
{
	char *argv[10];
	char *name;
	int res;
	memset(&argv, 0, sizeof(char*)*10);
	if (!PyArg_ParseTuple(args, "s|ssssss:print_event", &name,
			      &argv[0], &argv[1], &argv[2],
			      &argv[3], &argv[4], &argv[5],
			      &argv[6], &argv[7], &argv[8]))
		return NULL;
	BEGIN_RAGE_CALLS(ALLOW_THREADS);
	rage_set_context(ph, self->context);
	res = rage_emit_print(ph, name, argv[0], argv[1], argv[2],
					 argv[3], argv[4], argv[5],
					 argv[6], argv[7], argv[8]);
	END_RAGE_CALLS();
	return PyInt_FromLong(res);
}

static PyObject *
Context_get_info(ContextObject *self, PyObject *args)
{
	const char *info;
	char *name;
	if (!PyArg_ParseTuple(args, "s:get_info", &name))
		return NULL;
	BEGIN_RAGE_CALLS(NONE);
	rage_set_context(ph, self->context);
	info = rage_get_info(ph, name);
	END_RAGE_CALLS();
	if (info == NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	return PyString_FromString(info);
}

static PyObject *
Context_get_list(ContextObject *self, PyObject *args)
{
	PyObject *plugin = Plugin_GetCurrent();
	rage_context *saved_context = Plugin_GetContext(plugin);
	PyObject *ret;
	Plugin_SetContext(plugin, self->context);
	ret = Module_rage_get_list((PyObject*)self, args);
	Plugin_SetContext(plugin, saved_context);
	return ret;
}

static PyMethodDef Context_methods[] = {
	{"set", (PyCFunction) Context_set, METH_NOARGS},
	{"command", (PyCFunction) Context_command, METH_VARARGS},
	{"prnt", (PyCFunction) Context_prnt, METH_VARARGS},
	{"emit_print", (PyCFunction) Context_emit_print, METH_VARARGS},
	{"get_info", (PyCFunction) Context_get_info, METH_VARARGS},
	{"get_list", (PyCFunction) Context_get_list, METH_VARARGS},
	{NULL, NULL}
};

statichere PyTypeObject Context_Type = {
	PyObject_HEAD_INIT(NULL)
	0,			/*ob_size*/
	"rage.Context",	/*tp_name*/
	sizeof(ContextObject),	/*tp_basicsize*/
	0,			/*tp_itemsize*/
	Context_dealloc,        /*tp_dealloc*/
	0,			/*tp_print*/
	0,			/*tp_getattr*/
	0,			/*tp_setattr*/
	0,			/*tp_compare*/
	0,			/*tp_repr*/
	0,			/*tp_as_number*/
	0,			/*tp_as_sequence*/
	0,			/*tp_as_mapping*/
	0,			/*tp_hash*/
        0,                      /*tp_call*/
        0,                      /*tp_str*/
        PyObject_GenericGetAttr,/*tp_getattro*/
        PyObject_GenericSetAttr,/*tp_setattro*/
        0,                      /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT,     /*tp_flags*/
        0,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        Context_methods,        /*tp_methods*/
        0,                      /*tp_members*/
        0,                      /*tp_getset*/
        0,                      /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        0,                      /*tp_init*/
        PyType_GenericAlloc,    /*tp_alloc*/
        PyType_GenericNew,      /*tp_new*/
      	_PyObject_Del,          /*tp_free*/
        0,                      /*tp_is_gc*/
};

static PyObject *
Context_FromContext(rage_context *context)
{
	ContextObject *ctxobj = PyObject_New(ContextObject, &Context_Type);
	if (ctxobj != NULL)
		ctxobj->context = context;
	return (PyObject *) ctxobj;
}

static PyObject *
Context_FromServerAndChannel(char *server, char *channel)
{
	ContextObject *ctxobj;
	rage_context *context;
	BEGIN_RAGE_CALLS(NONE);
	context = rage_find_context(ph, server, channel);
	END_RAGE_CALLS();
	if (context == NULL)
		return NULL;
	ctxobj = PyObject_New(ContextObject, &Context_Type);
	if (ctxobj == NULL)
		return NULL;
	ctxobj->context = context;
	return (PyObject *) ctxobj;
}


/* ===================================================================== */
/* ListItem object */

#undef OFF
#define OFF(x) offsetof(ListItemObject, x)

static PyMemberDef ListItem_members[] = {
	{"__dict__", T_OBJECT, OFF(dict), 0},
	{0}
};

static void
ListItem_dealloc(PyObject *self)
{
	Py_DECREF(((ListItemObject*)self)->dict);
	self->ob_type->tp_free((PyObject *)self);
}

static PyObject *
ListItem_repr(PyObject *self)
{
	return PyString_FromFormat("<%s list item at %p>",
			    	   ((ListItemObject*)self)->listname, self);
}

statichere PyTypeObject ListItem_Type = {
	PyObject_HEAD_INIT(NULL)
	0,			/*ob_size*/
	"rage.ListItem",	/*tp_name*/
	sizeof(ListItemObject),	/*tp_basicsize*/
	0,			/*tp_itemsize*/
	ListItem_dealloc,	/*tp_dealloc*/
	0,			/*tp_print*/
	0,			/*tp_getattr*/
	0,			/*tp_setattr*/
	0,			/*tp_compare*/
	ListItem_repr,		/*tp_repr*/
	0,			/*tp_as_number*/
	0,			/*tp_as_sequence*/
	0,			/*tp_as_mapping*/
	0,			/*tp_hash*/
        0,                      /*tp_call*/
        0,                      /*tp_str*/
        PyObject_GenericGetAttr,/*tp_getattro*/
        PyObject_GenericSetAttr,/*tp_setattro*/
        0,                      /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT,     /*tp_flags*/
        0,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        0,                      /*tp_methods*/
        ListItem_members,       /*tp_members*/
        0,                      /*tp_getset*/
        0,                      /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        OFF(dict),              /*tp_dictoffset*/
        0,                      /*tp_init*/
        PyType_GenericAlloc,    /*tp_alloc*/
        PyType_GenericNew,      /*tp_new*/
      	_PyObject_Del,          /*tp_free*/
        0,                      /*tp_is_gc*/
};

static PyObject *
ListItem_New(const char *listname)
{
	ListItemObject *item;
	item = PyObject_New(ListItemObject, &ListItem_Type);
	if (item != NULL) {
		/* listname parameter must be statically allocated. */
		item->listname = listname;
		item->dict = PyDict_New();
		if (item->dict == NULL) {
			Py_DECREF(item);
			item = NULL;
		}
	}
	return (PyObject *) item;
}


/* ===================================================================== */
/* Plugin object */

#define GET_MODULE_DATA(x, force) \
	o = PyObject_GetAttrString(m, "__module_" #x "__"); \
	if (o == NULL) { \
		if (force) { \
			rage_print(ph, "Module has no __module_" #x "__ " \
					"defined"); \
			goto error; \
		} \
		plugin->x = g_strdup(""); \
	} else {\
		if (!PyString_Check(o)) { \
			rage_print(ph, "Variable __module_" #x "__ " \
					"must be a string"); \
			goto error; \
		} \
		plugin->x = g_strdup(PyString_AsString(o)); \
		if (plugin->x == NULL) { \
			rage_print(ph, "Not enough memory to allocate " #x); \
			goto error; \
		} \
	}

static PyObject *
Plugin_New(char *filename, PyMethodDef *rage_methods, PyObject *xcoobj)
{
	PluginObject *plugin = NULL;
	PyObject *m, *o;
	char *argv[] = {"<rage>", 0};

	if (filename) {
		char *old_filename = filename;
		filename = Util_Expand(filename);
		if (filename == NULL) {
			rage_printf(ph, "File not found: %s", old_filename);
			return NULL;
		}
	}

	/* Allocate plugin structure. */
	plugin = PyObject_New(PluginObject, &Plugin_Type);
	if (plugin == NULL) {
		rage_print(ph, "Can't create plugin object");
		goto error;
	}

	Plugin_SetName(plugin, NULL);
	Plugin_SetVersion(plugin, NULL);
	Plugin_SetFilename(plugin, NULL);
	Plugin_SetDescription(plugin, NULL);
	Plugin_SetHooks(plugin, NULL);
	Plugin_SetContext(plugin, rage_get_context(ph));

	/* Start a new interpreter environment for this plugin. */
	PyEval_AcquireLock();
	plugin->tstate = Py_NewInterpreter();
	if (plugin->tstate == NULL) {
		rage_print(ph, "Can't create interpreter state");
		goto error;
	}

	PySys_SetArgv(1, argv);
	PySys_SetObject("__plugin__", (PyObject *) plugin);

	/* Set stdout and stderr to rageout. */
	Py_INCREF(xcoobj);
	PySys_SetObject("stdout", xcoobj);
	Py_INCREF(xcoobj);
	PySys_SetObject("stderr", xcoobj);

	/* Add rage module to the environment. */
	m = Py_InitModule("rage", rage_methods);
	if (m == NULL) {
		rage_print(ph, "Can't create rage module");
		goto error;
	}

	PyModule_AddIntConstant(m, "EAT_NONE", RAGE_EAT_NONE);
	PyModule_AddIntConstant(m, "EAT_RAGE", RAGE_EAT_RAGE);
	PyModule_AddIntConstant(m, "EAT_PLUGIN", RAGE_EAT_PLUGIN);
	PyModule_AddIntConstant(m, "EAT_ALL", RAGE_EAT_ALL);
	PyModule_AddIntConstant(m, "PRI_HIGHEST", RAGE_PRI_HIGHEST);
	PyModule_AddIntConstant(m, "PRI_HIGH", RAGE_PRI_HIGH);
	PyModule_AddIntConstant(m, "PRI_NORM", RAGE_PRI_NORM);
	PyModule_AddIntConstant(m, "PRI_LOW", RAGE_PRI_LOW);
	PyModule_AddIntConstant(m, "PRI_LOWEST", RAGE_PRI_LOWEST);

	o = Py_BuildValue("(ii)", VERSION_MAJOR, VERSION_MINOR);
	if (o == NULL) {
		rage_print(ph, "Can't create version tuple");
		goto error;
	}
	PyObject_SetAttrString(m, "__version__", o);

	if (filename) {
		FILE *fp;

		plugin->filename = filename;

		/* It's now owned by the plugin. */
		filename = NULL;

		/* Open the plugin file. */
		fp = fopen(plugin->filename, "r");
		if (fp == NULL) {
			rage_printf(ph, "Can't open file %s: %s\n",
				     filename, strerror(errno));
			goto error;
		}

		/* Run the plugin. */
		if (PyRun_SimpleFile(fp, plugin->filename) != 0) {
			rage_printf(ph, "Error loading module %s\n",
				     plugin->filename);
			fclose(fp);
			goto error;
		}
		fclose(fp);

		m = PyDict_GetItemString(PyImport_GetModuleDict(),
					 "__main__");
		if (m == NULL) {
			rage_print(ph, "Can't get __main__ module");
			goto error;
		}
		GET_MODULE_DATA(name, 1);
		GET_MODULE_DATA(version, 0);
		GET_MODULE_DATA(description, 0);
		plugin->gui = rage_plugingui_add(ph, plugin->filename,
						  plugin->name,
						  plugin->description,
						  plugin->version, NULL);
	}

	PyEval_ReleaseThread(plugin->tstate);

	return (PyObject *) plugin;

error:
	g_free(filename);

	if (plugin) {
		if (plugin->tstate)
			Py_EndInterpreter(plugin->tstate);
		Py_DECREF(plugin);
	}
	PyEval_ReleaseLock();

	return NULL;
}

static PyObject *
Plugin_GetCurrent(void)
{
	PyObject *plugin;
	plugin = PySys_GetObject("__plugin__");
	if (plugin == NULL)
		PyErr_SetString(PyExc_RuntimeError, "lost sys.__plugin__");
	return plugin;
}

static PluginObject *
Plugin_ByString(char *str)
{
	GSList *list;
	PluginObject *plugin;
	char *basename;
	list = plugin_list;
	while (list != NULL) {
		plugin = (PluginObject *) list->data;
		basename = g_path_get_basename(plugin->filename);
		if (basename == NULL)
			break;
		if (strcasecmp(plugin->name, str) == 0 ||
		    strcasecmp(plugin->filename, str) == 0 ||
		    strcasecmp(basename, str) == 0) {
			g_free(basename);
			return plugin;
		}
		g_free(basename);
		list = list->next;
	}
	return NULL;
}

static Hook *
Plugin_AddHook(int type, PyObject *plugin, PyObject *callback,
	       PyObject *userdata, void *data)
{
	Hook *hook = (Hook *) g_malloc(sizeof(Hook));
	if (hook == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	hook->type = type;
	hook->plugin = plugin;
	Py_INCREF(callback);
	hook->callback = callback;
	Py_INCREF(userdata);
	hook->userdata = userdata;
	hook->data = NULL;
	Plugin_SetHooks(plugin, g_slist_append(Plugin_GetHooks(plugin),
					       hook));

	return hook;
}

static void
Plugin_RemoveHook(PyObject *plugin, Hook *hook)
{
	GSList *list;
	/* Is this really a hook of the running plugin? */
	list = g_slist_find(Plugin_GetHooks(plugin), hook);
	if (list) {
		/* Ok, unhook it. */
		if (hook->type == HOOK_RAGE) {
			/* This is an rage hook. Unregister it. */
			BEGIN_RAGE_CALLS(NONE);
			rage_unhook(ph, (rage_hook*)hook->data);
			END_RAGE_CALLS();
		}
		Plugin_SetHooks(plugin,
				g_slist_remove(Plugin_GetHooks(plugin),
					       hook));
		Py_DECREF(hook->callback);
		Py_DECREF(hook->userdata);
		g_free(hook);
	}
}

static void
Plugin_RemoveAllHooks(PyObject *plugin)
{
	GSList *list = Plugin_GetHooks(plugin);
	while (list) {
		Hook *hook = (Hook *) list->data;
		if (hook->type == HOOK_RAGE) {
			/* This is an rage hook. Unregister it. */
			BEGIN_RAGE_CALLS(NONE);
			rage_unhook(ph, (rage_hook*)hook->data);
			END_RAGE_CALLS();
		}
		Py_DECREF(hook->callback);
		Py_DECREF(hook->userdata);
		g_free(hook);
		list = list->next;
	}
	Plugin_SetHooks(plugin, NULL);
}

static void
Plugin_Delete(PyObject *plugin)
{
	PyThreadState *tstate = ((PluginObject*)plugin)->tstate;
	GSList *list = Plugin_GetHooks(plugin);
	while (list) {
		Hook *hook = (Hook *) list->data;
		if (hook->type == HOOK_UNLOAD) {
			PyObject *retobj;
			retobj = PyObject_CallFunction(hook->callback, "(O)",
						       hook->userdata);
			if (retobj) {
				Py_DECREF(retobj);
			} else {
				PyErr_Print();
				PyErr_Clear();
			}
		}
		list = list->next;
	}
	Plugin_RemoveAllHooks(plugin);
	rage_plugingui_remove(ph, ((PluginObject *)plugin)->gui);
	Py_DECREF(plugin);
	Py_EndInterpreter(tstate);
}

static void
Plugin_dealloc(PluginObject *self)
{
	g_free(self->filename);
	g_free(self->name);
	g_free(self->version);
	g_free(self->description);
	self->ob_type->tp_free((PyObject *)self);
}

statichere PyTypeObject Plugin_Type = {
	PyObject_HEAD_INIT(NULL)
	0,			/*ob_size*/
	"rage.Plugin",		/*tp_name*/
	sizeof(PluginObject),	/*tp_basicsize*/
	0,			/*tp_itemsize*/
	(destructor)Plugin_dealloc, /*tp_dealloc*/
	0,			/*tp_print*/
	0,			/*tp_getattr*/
	0,			/*tp_setattr*/
	0,			/*tp_compare*/
	0,			/*tp_repr*/
	0,			/*tp_as_number*/
	0,			/*tp_as_sequence*/
	0,			/*tp_as_mapping*/
	0,			/*tp_hash*/
        0,                      /*tp_call*/
        0,                      /*tp_str*/
        PyObject_GenericGetAttr,/*tp_getattro*/
        PyObject_GenericSetAttr,/*tp_setattro*/
        0,                      /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT,     /*tp_flags*/
        0,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        0,                      /*tp_methods*/
        0,                      /*tp_members*/
        0,                      /*tp_getset*/
        0,                      /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        0,                      /*tp_init*/
        PyType_GenericAlloc,    /*tp_alloc*/
        PyType_GenericNew,      /*tp_new*/
      	_PyObject_Del,          /*tp_free*/
        0,                      /*tp_is_gc*/
};


/* ===================================================================== */
/* Rage module */

static PyObject *
Module_rage_command(PyObject *self, PyObject *args)
{
	char *text;
	if (!PyArg_ParseTuple(args, "s:command", &text))
		return NULL;
	BEGIN_RAGE_CALLS(RESTORE_CONTEXT|ALLOW_THREADS);
	rage_command(ph, text);
	END_RAGE_CALLS();
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
Module_rage_prnt(PyObject *self, PyObject *args)
{
	char *text;
	if (!PyArg_ParseTuple(args, "s:prnt", &text))
		return NULL;
	BEGIN_RAGE_CALLS(RESTORE_CONTEXT|ALLOW_THREADS);
	rage_print(ph, text);
	END_RAGE_CALLS();
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
Module_rage_emit_print(PyObject *self, PyObject *args)
{
	char *argv[10];
	char *name;
	int res;
	memset(&argv, 0, sizeof(char*)*10);
	if (!PyArg_ParseTuple(args, "s|ssssss:print_event", &name,
			      &argv[0], &argv[1], &argv[2],
			      &argv[3], &argv[4], &argv[5],
			      &argv[6], &argv[7], &argv[8]))
		return NULL;
	BEGIN_RAGE_CALLS(RESTORE_CONTEXT|ALLOW_THREADS);
	res = rage_emit_print(ph, name, argv[0], argv[1], argv[2],
					 argv[3], argv[4], argv[5],
					 argv[6], argv[7], argv[8]);
	END_RAGE_CALLS();
	return PyInt_FromLong(res);
}

static PyObject *
Module_rage_get_info(PyObject *self, PyObject *args)
{
	const char *info;
	char *name;
	if (!PyArg_ParseTuple(args, "s:get_info", &name))
		return NULL;
	BEGIN_RAGE_CALLS(RESTORE_CONTEXT);
	info = rage_get_info(ph, name);
	END_RAGE_CALLS();
	if (info == NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	return PyString_FromString(info);
}

/* My god this sucks */
static PyObject *
Module_rage_get_prefs(PyObject *self, PyObject *args)
{
	PyObject *res;
	const char *info;
	char *name;
	int type;
	if (!PyArg_ParseTuple(args, "s:get_prefs", &name))
		return NULL;
	BEGIN_RAGE_CALLS(NONE);
	type = rage_get_prefs(ph, name, (const char **)&info, &type);
	END_RAGE_CALLS();
	switch (type) {
		case 0:
			Py_INCREF(Py_None);
			res = Py_None;
			break;
		case 1:
			res = PyString_FromString((char*)info);
			break;
		case 2:
		case 3: /* Is this even 64bit safe?! */
			res = PyInt_FromLong((int)info);
			break;
		default:
			PyErr_Format(PyExc_RuntimeError,
				     "unknown get_prefs type (%d), "
				     "please report", type);
			res = NULL;
			break;
	}
	return res;
}

static PyObject *
Module_rage_get_context(PyObject *self, PyObject *args)
{
	PyObject *plugin;
	PyObject *ctxobj;
	plugin = Plugin_GetCurrent();
	if (plugin == NULL)
		return NULL;
	ctxobj = Context_FromContext(Plugin_GetContext(plugin));
	if (ctxobj == NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	return ctxobj;
}

static PyObject *
Module_rage_find_context(PyObject *self, PyObject *args, PyObject *kwargs)
{
	char *server = NULL;
	char *channel = NULL;
	PyObject *ctxobj;
	char *kwlist[] = {"server", "channel", 0};
	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|zz:find_context",
					 kwlist, &server, &channel))
		return NULL;
	ctxobj = Context_FromServerAndChannel(server, channel);
	if (ctxobj == NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	return ctxobj;
}

static PyObject *
Module_rage_hook_command(PyObject *self, PyObject *args, PyObject *kwargs)
{
	char *name;
	PyObject *callback;
	PyObject *userdata = Py_None;
	int priority = RAGE_PRI_NORM;
	char *help = NULL;
	PyObject *plugin;
	Hook *hook;
	char *kwlist[] = {"name", "callback", "userdata",
			  "priority", "help", 0};

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO|Oiz:hook_command",
					 kwlist, &name, &callback, &userdata,
					 &priority, &help))
		return NULL;

	plugin = Plugin_GetCurrent();
	if (plugin == NULL)
		return NULL;
	if (!PyCallable_Check(callback)) {
		PyErr_SetString(PyExc_TypeError, "callback is not callable");
		return NULL;
	}

	hook = Plugin_AddHook(HOOK_RAGE, plugin, callback, userdata, NULL);
	if (hook == NULL)
		return NULL;

	BEGIN_RAGE_CALLS(NONE);
	hook->data = (void*)rage_hook_command(ph, name, priority,
					       Callback_Command, help, hook);
	END_RAGE_CALLS();

	return PyInt_FromLong((long)hook);
}

static PyObject *
Module_rage_hook_server(PyObject *self, PyObject *args, PyObject *kwargs)
{
	char *name;
	PyObject *callback;
	PyObject *userdata = Py_None;
	int priority = RAGE_PRI_NORM;
	PyObject *plugin;
	Hook *hook;
	char *kwlist[] = {"name", "callback", "userdata", "priority", 0};

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO|Oi:hook_server",
					 kwlist, &name, &callback, &userdata,
					 &priority))
		return NULL;

	plugin = Plugin_GetCurrent();
	if (plugin == NULL)
		return NULL;
	if (!PyCallable_Check(callback)) {
		PyErr_SetString(PyExc_TypeError, "callback is not callable");
		return NULL;
	}

	hook = Plugin_AddHook(HOOK_RAGE, plugin, callback, userdata, NULL);
	if (hook == NULL)
		return NULL;

	BEGIN_RAGE_CALLS(NONE);
	hook->data = (void*)rage_hook_server(ph, name, priority,
					      Callback_Command, hook);
	END_RAGE_CALLS();

	return PyInt_FromLong((long)hook);
}

static PyObject *
Module_rage_hook_print(PyObject *self, PyObject *args, PyObject *kwargs)
{
	char *name;
	PyObject *callback;
	PyObject *userdata = Py_None;
	int priority = RAGE_PRI_NORM;
	PyObject *plugin;
	Hook *hook;
	char *kwlist[] = {"name", "callback", "userdata", "priority", 0};

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO|Oi:hook_print",
					 kwlist, &name, &callback, &userdata,
					 &priority))
		return NULL;

	plugin = Plugin_GetCurrent();
	if (plugin == NULL)
		return NULL;
	if (!PyCallable_Check(callback)) {
		PyErr_SetString(PyExc_TypeError, "callback is not callable");
		return NULL;
	}

	hook = Plugin_AddHook(HOOK_RAGE, plugin, callback, userdata, NULL);
	if (hook == NULL)
		return NULL;

	BEGIN_RAGE_CALLS(NONE);
	hook->data = (void*)rage_hook_print(ph, name, priority,
					     Callback_Print, hook);
	END_RAGE_CALLS();

	return PyInt_FromLong((long)hook);
}


static PyObject *
Module_rage_hook_timer(PyObject *self, PyObject *args, PyObject *kwargs)
{
	int timeout;
	PyObject *callback;
	PyObject *userdata = Py_None;
	PyObject *plugin;
	Hook *hook;
	char *kwlist[] = {"timeout", "callback", "userdata", 0};

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "iO|O:hook_timer",
					 kwlist, &timeout, &callback,
					 &userdata))
		return NULL;

	plugin = Plugin_GetCurrent();
	if (plugin == NULL)
		return NULL;
	if (!PyCallable_Check(callback)) {
		PyErr_SetString(PyExc_TypeError, "callback is not callable");
		return NULL;
	}

	hook = Plugin_AddHook(HOOK_RAGE, plugin, callback, userdata, NULL);
	if (hook == NULL)
		return NULL;

	BEGIN_RAGE_CALLS(NONE);
	hook->data = (void*)rage_hook_timer(ph, timeout,
					     Callback_Timer, hook);
	END_RAGE_CALLS();

	return PyInt_FromLong((long)hook);
}

static PyObject *
Module_rage_hook_unload(PyObject *self, PyObject *args, PyObject *kwargs)
{
	PyObject *callback;
	PyObject *userdata = Py_None;
	PyObject *plugin;
	Hook *hook;
	char *kwlist[] = {"callback", "userdata", 0};

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O:hook_unload",
					 kwlist, &callback, &userdata))
		return NULL;

	plugin = Plugin_GetCurrent();
	if (plugin == NULL)
		return NULL;
	if (!PyCallable_Check(callback)) {
		PyErr_SetString(PyExc_TypeError, "callback is not callable");
		return NULL;
	}

	hook = Plugin_AddHook(HOOK_UNLOAD, plugin, callback, userdata, NULL);
	if (hook == NULL)
		return NULL;

	return PyInt_FromLong((long)hook);
}

static PyObject *
Module_rage_unhook(PyObject *self, PyObject *args)
{
	PyObject *plugin;
	Hook *hook;
	if (!PyArg_ParseTuple(args, "l:unhook", &hook))
		return NULL;
	plugin = Plugin_GetCurrent();
	if (plugin == NULL)
		return NULL;
	Plugin_RemoveHook(plugin, hook);
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
Module_rage_get_list(PyObject *self, PyObject *args)
{
	rage_list *list;
	PyObject *l;
	const char *name;
	const char **fields;
	int i;

	if (!PyArg_ParseTuple(args, "s:get_list", &name))
		return NULL;
	/* This function is thread safe, and returns statically
	 * allocated data. */
	fields = rage_list_fields(ph, "lists");
	for (i = 0; fields[i]; i++) {
		if (strcmp(fields[i], name) == 0) {
			/* Use the static allocated one. */
			name = fields[i];
			break;
		}
	}
	if (fields[i] == NULL) {
		PyErr_SetString(PyExc_KeyError, "list not available");
		return NULL;
	}
	l = PyList_New(0);
	if (l == NULL)
		return NULL;
	BEGIN_RAGE_CALLS(RESTORE_CONTEXT);
	list = rage_list_get(ph, (char*)name);
	if (list == NULL)
		goto error;
	fields = rage_list_fields(ph, (char*)name);
	while (rage_list_next(ph, list)) {
		PyObject *o = ListItem_New(name);
		if (o == NULL || PyList_Append(l, o) == -1) {
			Py_XDECREF(o);
			goto error;
		}
		Py_DECREF(o); /* l is holding a reference */
		for (i = 0; fields[i]; i++) {
			const char *fld = fields[i]+1;
			PyObject *attr = NULL;
			const char *sattr;
			int iattr;
			switch(fields[i][0]) {
			case 's':
				sattr = rage_list_str(ph, list, (char*)fld);
				attr = PyString_FromString(sattr?sattr:"");
				break;
			case 'i':
				iattr = rage_list_int(ph, list, (char*)fld);
				attr = PyInt_FromLong((long)iattr);
				break;
			case 'p':
				sattr = rage_list_str(ph, list, (char*)fld);
				if (strcmp(fld, "context") == 0) {
					attr = Context_FromContext(
						(rage_context*)sattr);
					break;
				}
				continue;
			}
			if (attr == NULL)
				goto error;
			PyObject_SetAttrString(o, (char*)fld, attr);
		}
	}
	rage_list_free(ph, list);
	goto exit;
error:
	if (list)
		rage_list_free(ph, list);
	Py_DECREF(l);
	l = NULL;

exit:
	END_RAGE_CALLS();
	return l;
}

static PyObject *
Module_rage_get_lists(PyObject *self, PyObject *args)
{
	PyObject *l, *o;
	const char **fields;
	int i;
	/* This function is thread safe, and returns statically
	 * allocated data. */
	fields = rage_list_fields(ph, "lists");
	l = PyList_New(0);
	if (l == NULL)
		return NULL;
	for (i = 0; fields[i]; i++) {
		o = PyString_FromString(fields[i]);
		if (o == NULL || PyList_Append(l, o) == -1) {
			Py_DECREF(l);
			Py_XDECREF(o);
			return NULL;
		}
		Py_DECREF(o); /* l is holding a reference */
	}
	return l;
}

static PyObject *
Module_rage_nickcmp(PyObject *self, PyObject *args)
{
	char *s1, *s2;
	if (!PyArg_ParseTuple(args, "ss:nickcmp", &s1, &s2))
		return NULL;
	return PyInt_FromLong((long) rage_nickcmp(ph, s1, s2));
}

static PyMethodDef Module_rage_methods[] = {
	{"command",		Module_rage_command,
		METH_VARARGS},
	{"prnt",		Module_rage_prnt,
		METH_VARARGS},
	{"emit_print",		Module_rage_emit_print,
		METH_VARARGS},
	{"get_info",		Module_rage_get_info,
		METH_VARARGS},
	{"get_prefs",		Module_rage_get_prefs,
		METH_VARARGS},
	{"get_context",		Module_rage_get_context,
		METH_NOARGS},
	{"find_context",	(PyCFunction)Module_rage_find_context,
		METH_VARARGS|METH_KEYWORDS},
	{"hook_command",	(PyCFunction)Module_rage_hook_command,
		METH_VARARGS|METH_KEYWORDS},
	{"hook_server",		(PyCFunction)Module_rage_hook_server,
		METH_VARARGS|METH_KEYWORDS},
	{"hook_print",		(PyCFunction)Module_rage_hook_print,
		METH_VARARGS|METH_KEYWORDS},
	{"hook_timer",		(PyCFunction)Module_rage_hook_timer,
		METH_VARARGS|METH_KEYWORDS},
	{"hook_unload",		(PyCFunction)Module_rage_hook_unload,
		METH_VARARGS|METH_KEYWORDS},
	{"unhook",		Module_rage_unhook,
		METH_VARARGS},
	{"get_list",		Module_rage_get_list,
		METH_VARARGS},
	{"get_lists",		Module_rage_get_lists,
		METH_NOARGS},
	{"nickcmp",		Module_rage_nickcmp,
		METH_VARARGS},
	{NULL, NULL}
};


/* ===================================================================== */
/* Python interactive interpreter functions */

static void
IInterp_Exec(char *command)
{
        PyObject *m, *d, *o;
	char *buffer;
	int len;

	BEGIN_PLUGIN(interp_plugin);

        m = PyImport_AddModule("__main__");
        if (m == NULL) {
		rage_print(ph, "Can't get __main__ module");
		goto fail;
	}
        d = PyModule_GetDict(m);
	len = strlen(command);
	buffer = (char *) g_malloc(len+2);
	if (buffer == NULL) {
		rage_print(ph, "Not enough memory for command buffer");
		goto fail;
	}
	memcpy(buffer, command, len);
	buffer[len] = '\n';
	buffer[len+1] = 0;
        o = PyRun_StringFlags(buffer, Py_single_input, d, d, NULL);
	g_free(buffer);
        if (o == NULL) {
                PyErr_Print();
		goto fail;
        }
        Py_DECREF(o);
        if (Py_FlushLine())
                PyErr_Clear();

fail:
	END_PLUGIN(interp_plugin);

        return;
}

static int
IInterp_Cmd(int parc, char *parv[], void *userdata)
{
	char *channel = (char *) rage_get_info(ph, "channel");
	g_return_val_if_fail(channel != NULL, 0);
	if (channel[0] == '>' && strcmp(channel, ">>python<<") == 0) {
		rage_printf(ph, ">>> %s\n", parv[1]);
		IInterp_Exec(parv[1]);
		return 1;
	}
	return 0;
}


/* ===================================================================== */
/* Python command handling */

static void
Command_PyList(void)
{
	GSList *list;
	list = plugin_list;
	if (list == NULL) {
		rage_print(ph, "No python modules loaded");
	} else {
		rage_print(ph,
		   "Name         Version  Filename             Description\n"
		   "----         -------  --------             -----------\n");
		while (list != NULL) {
			PluginObject *plg = (PluginObject *) list->data;
			char *basename = g_path_get_basename(plg->filename);
			rage_printf(ph, "%-12s %-8s %-20s %-10s\n",
				     plg->name,
				     *plg->version ? plg->version
				     		  : "<none>",
				     basename,
				     *plg->description ? plg->description
				     		      : "<none>");
			g_free(basename);
			list = list->next;
		}
		rage_print(ph, "\n");
	}
}

static void
Command_PyLoad(char *filename)
{
	PyObject *plugin;
	RELEASE_RAGE_LOCK();
	plugin = Plugin_New(filename, Module_rage_methods, rageout);
	ACQUIRE_RAGE_LOCK();
	if (plugin)
		plugin_list = g_slist_append(plugin_list, plugin);
}

static void
Command_PyUnload(char *name)
{
	PluginObject *plugin = Plugin_ByString(name);
	if (!plugin) {
		rage_print(ph, "Can't find a python plugin with that name");
	} else {
		BEGIN_PLUGIN(plugin);
		Plugin_Delete((PyObject*)plugin);
		END_PLUGIN(plugin);
		plugin_list = g_slist_remove(plugin_list, plugin);
	}
}

static void
Command_PyReload(char *name)
{
	PluginObject *plugin = Plugin_ByString(name);
	if (!plugin) {
		rage_print(ph, "Can't find a python plugin with that name");
	} else {
		char *filename = strdup(plugin->filename);
		Command_PyUnload(filename);
		Command_PyLoad(filename);
		g_free(filename);
	}
}

static void
Command_PyAbout(void)
{
	rage_print(ph, about);
}

static int
Command_Py(int parc, char *word[], void *userdata)
{
	char cmd[32];
	int ok = 0;
	char *p;
	int i;

	p=word[1];
	while(*p && *p!=' ')p++;
	i=0;
	while(*p && *p==' ')p++;
	while(*p && *p!=' ' && i<sizeof(cmd)-1) {
		cmd[i++]=*p;
		p++;
	}
	cmd[i]='\0';
	while(*p && *p==' ')p++;

	if (strcasecmp(cmd, "LIST") == 0) {
		ok = 1;
		Command_PyList();
	} else if (strcasecmp(cmd, "EXEC") == 0) {
		if (p) {
			ok = 1;
			IInterp_Exec(p); 
		}
	} else if (strcasecmp(cmd, "LOAD") == 0) {
		if (p) {
			ok = 1;
			Command_PyLoad(p);
		}
	} else if (strcasecmp(cmd, "UNLOAD") == 0) {
		if (p) {
			ok = 1;
			Command_PyUnload(p);
		}
	} else if (strcasecmp(cmd, "RELOAD") == 0) {
		if (p) {
			ok = 1;
			Command_PyReload(p);
		}
	} else if (strcasecmp(cmd, "CONSOLE") == 0) {
		ok = 1;
		rage_command(ph, "QUERY >>python<<");
	} else if (strcasecmp(cmd, "ABOUT") == 0) {
		ok = 1;
		Command_PyAbout();
	}
	if (!ok)
		rage_print(ph, usage);
	return RAGE_EAT_ALL;
}

static int
Command_Load(int parc, char *word[], void *userdata)
{
	int len = strlen(word[2]);
	if (len > 3 && strcasecmp(".py", word[2]+len-3) == 0) {
		Command_PyLoad(word[2]);
		return RAGE_EAT_RAGE;
	}
	return RAGE_EAT_NONE;
}

static int
Command_Unload(int parc, char *word[], void *userdata)
{
	int len = strlen(word[2]);
	if (len > 3 && strcasecmp(".py", word[2]+len-3) == 0) {
		Command_PyUnload(word[2]);
		return RAGE_EAT_RAGE;
	}
	return RAGE_EAT_NONE;
}

/* ===================================================================== */
/* Autoload function */

/* ===================================================================== */
/* (De)initialization functions */

static int initialized = 0;
static int reinit_tried = 0;

int
rage_plugin_init(rage_plugin *plugin_handle,
		  char **plugin_name,
		  char **plugin_desc,
		  char **plugin_version,
		  char *arg)
{
	char *argv[] = {"<rage>", 0};

	ph = plugin_handle;

	/* Block double initalization. */
	if (initialized != 0) {
		rage_print(ph, "Python interface already loaded");
		/* deinit is called even when init fails, so keep track
		 * of a reinit failure. */
		reinit_tried++;
		return 0;
	}
	initialized = 1;

	*plugin_name = "Python";
	*plugin_version = VERSION;
	*plugin_desc = "Python scripting interface";

	/* Initialize python. */
	Py_SetProgramName("rage");
	Py_Initialize();
	PySys_SetArgv(1, argv);

	Plugin_Type.ob_type = &PyType_Type;
	Context_Type.ob_type = &PyType_Type;
	RageOut_Type.ob_type = &PyType_Type;

	rageout = RageOut_New();
	if (rageout == NULL) {
		rage_print(ph, "Can't allocate rageout object");
		return 0;
	}

#ifdef WITH_THREAD
	PyEval_InitThreads();
	rage_lock = PyThread_allocate_lock();
	if (rage_lock == NULL) {
		rage_print(ph, "Can't allocate rage lock");
		Py_DECREF(rageout);
		rageout = NULL;
		return 0;
	}
#endif

	main_tstate = PyEval_SaveThread();

	interp_plugin = Plugin_New(NULL, Module_rage_methods, rageout);
	if (interp_plugin == NULL) {
		rage_print(ph, "Plugin_New() failed.\n");
#ifdef WITH_THREAD
		PyThread_free_lock(rage_lock);
#endif
		Py_DECREF(rageout);
		rageout = NULL;
		return 0;
	}


	rage_hook_command(ph, "", RAGE_PRI_NORM, IInterp_Cmd, 0, 0);
	rage_hook_command(ph, "PY", RAGE_PRI_NORM, Command_Py, usage, 0);
	rage_hook_command(ph, "LOAD", RAGE_PRI_NORM, Command_Load, 0, 0);
	rage_hook_command(ph, "UNLOAD", RAGE_PRI_NORM, Command_Unload, 0, 0);
#ifdef WITH_THREAD
	thread_timer = rage_hook_timer(ph, 300, Callback_ThreadTimer, NULL);
#endif

	rage_print(ph, "Python interface loaded\n");

	Util_Autoload();
	return 1;
}

int
rage_plugin_deinit(void)
{
	GSList *list;

	/* A reinitialization was tried. Just give up and live the
	 * environment as is. We are still alive. */
	if (reinit_tried) {
		reinit_tried--;
		return 1;
	}

	list = plugin_list;
	while (list != NULL) {
		PyObject *plugin = (PyObject *) list->data;
		BEGIN_PLUGIN(plugin);
		Plugin_Delete(plugin);
		END_PLUGIN(plugin);
		list = list->next;
	}
	g_slist_free(plugin_list);
	plugin_list = NULL;

	/* Reset rageout buffer. */
	g_free(rageout_buffer);
	rageout_buffer = NULL;
	rageout_buffer_size = 0;
	rageout_buffer_pos = 0;

	if (interp_plugin) {
		Py_DECREF(interp_plugin);
		interp_plugin = NULL;
	}

	/* Switch back to the main thread state. */
	if (main_tstate) {
		PyThreadState_Swap(main_tstate);
		main_tstate = NULL;
	}
	Py_Finalize();

#ifdef WITH_THREAD
	if (thread_timer != NULL) {
		rage_unhook(ph, thread_timer);
		thread_timer = NULL;
	}
	PyThread_free_lock(rage_lock);
#endif

	rage_print(ph, "Python interface unloaded\n");
	initialized = 0;

	return 1;
}

