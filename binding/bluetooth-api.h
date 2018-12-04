/*
 * Copyright 2018 Konsulko Group
 * Author: Matt Ranostay <matt.ranostay@konsulko.com>
 * Author: Pantelis Antoniou <pantelis.antoniou@konsulko.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BLUETOOTH_API_H
#define BLUETOOTH_API_H

#include <alloca.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include <glib.h>

#include <json-c/json.h>

#define BLUEZ_SERVICE                 		"org.bluez"
#define BLUEZ_ADAPTER_INTERFACE			BLUEZ_SERVICE ".Adapter1"
#define BLUEZ_AGENT_INTERFACE			BLUEZ_SERVICE ".Agent1"
#define BLUEZ_AGENTMANAGER_INTERFACE		BLUEZ_SERVICE ".AgentManager1"
#define BLUEZ_DEVICE_INTERFACE			BLUEZ_SERVICE ".Device1"
#define BLUEZ_MEDIAPLAYER_INTERFACE		BLUEZ_SERVICE ".MediaPlayer1"
#define BLUEZ_MEDIATRANSPORT_INTERFACE		BLUEZ_SERVICE ".MediaTransport1"

#define BLUEZ_OBJECT_PATH			"/"
#define BLUEZ_PATH				"/org/bluez"

#define BLUEZ_ROOT_PATH(_t) \
    ({ \
     call_work_lock(ns); \
     const char *__t = (_t); \
     size_t __len = strlen(BLUEZ_PATH) + 1 + \
     strlen(__t) + 1; \
     char *__tpath; \
     __tpath = alloca(__len + 1 + 1); \
     snprintf(__tpath, __len + 1, \
             BLUEZ_PATH "/%s", __t); \
     call_work_unlock(ns); \
             __tpath; \
     })

#define BLUEZ_ERRMSG(error) \
     (error ? error->message : "unspecified")

#define FREEDESKTOP_INTROSPECT			"org.freedesktop.DBus.Introspectable"
#define FREEDESKTOP_PROPERTIES			"org.freedesktop.DBus.Properties"
#define FREEDESKTOP_OBJECTMANAGER		"org.freedesktop.DBus.ObjectManager"

#define DBUS_REPLY_TIMEOUT			(120 * 1000)
#define DBUS_REPLY_TIMEOUT_SHORT		(10 * 1000)

#define BLUEZ_AT_OBJECT				"object"
#define BLUEZ_AT_ADAPTER			"adapter"
#define BLUEZ_AT_DEVICE				"device"
#define BLUEZ_AT_AGENT				"agent"
#define BLUEZ_AT_AGENTMANAGER			"agent-manager"
#define BLUEZ_AT_MEDIAPLAYER			"mediaplayer"
#define BLUEZ_AT_MEDIATRANSPORT			"mediatransport"

#define BLUEZ_DEFAULT_ADAPTER			"hci0"
#define BLUEZ_DEFAULT_PLAYER			"player0"

struct bluetooth_state;


static inline int split_length(const char *path) {
	gchar **strings = g_strsplit(path, "/", -1);
	int ret = g_strv_length(strings) ;

	g_strfreev(strings);
	return ret;
}

static inline gchar *find_index(const char *path, int idx)
{
	gchar **strings = g_strsplit(path, "/", -1);
	gchar *item = NULL;

	if (g_strv_length(strings) > idx)
		item = g_strdup(strings[idx]);

	g_strfreev(strings);
	return item;
}

static inline gchar *bluez_return_adapter(const char *path)
{
	return find_index(path, 3);
}

static inline gchar *bluez_return_device(const char *path)
{
	return find_index(path, 4);
}

static inline gboolean is_mediaplayer1_interface(const char *path)
{
	gchar *data = NULL;
	gboolean ret;

	// Don't trigger on NowPlaying, Item, etc paths
	if (split_length(path) != 6)
		return FALSE;

	// TODO: allow mutiple players per device
	data = find_index(path, 5);
	ret = !g_strcmp0(data, BLUEZ_DEFAULT_PLAYER);
	g_free(data);

	return ret;
}

static inline gboolean is_mediatransport1_interface(const char *path)
{
	gchar *data = NULL;
	gboolean ret;

	// Don't trigger on NowPlaying, Item, etc paths
	if (split_length(path) != 6)
		return FALSE;

	data = find_index(path, 5);
	ret = g_str_has_prefix(data, "fd");
	g_free(data);

	return ret;
}

struct bluetooth_state *bluetooth_get_userdata(afb_req_t request);

struct call_work *call_work_create_unlocked(struct bluetooth_state *ns,
		const char *access_type, const char *type_arg,
		const char *method, const char *bluez_method,
		GError **error);

void call_work_destroy_unlocked(struct call_work *cw);

void call_work_lock(struct bluetooth_state *ns);

void call_work_unlock(struct bluetooth_state *ns);

struct call_work *call_work_lookup_unlocked(
		struct bluetooth_state *ns,
		const char *access_type, const char *type_arg,
		const char *method);

const struct property_info *bluez_get_property_info(
		const char *access_type, GError **error);

gboolean bluez_property_dbus2json(const char *access_type,
		json_object *jprop, const gchar *key, GVariant *var,
		gboolean *is_config,
		GError **error);

GVariant *bluez_call(struct bluetooth_state *ns,
		const char *access_type, const char *type_arg,
		const char *method, GVariant *params, GError **error);

json_object *bluez_get_properties(struct bluetooth_state *ns,
		const char *access_type, const char *path,
		GError **error);

json_object *bluez_get_property(struct bluetooth_state *ns,
		const char *access_type, const char *type_arg,
		gboolean is_json_name, const char *name, GError **error);

gboolean bluez_set_property(struct bluetooth_state *ns,
		const char *access_type, const char *type_arg,
		gboolean is_json_name, const char *name, json_object *jval,
		GError **error);

gboolean bluetooth_autoconnect(gpointer data);

/* convenience access methods */
static inline gboolean device_property_dbus2json(json_object *jprop,
		const gchar *key, GVariant *var, gboolean *is_config,
		GError **error)
{
	return bluez_property_dbus2json(BLUEZ_AT_DEVICE,
			jprop, key, var, is_config, error);
}

static inline gboolean agent_property_dbus2json(json_object *jprop,
		const gchar *key, GVariant *var, gboolean *is_config,
		GError **error)
{
	return bluez_property_dbus2json(BLUEZ_AT_AGENT,
			jprop, key, var, is_config, error);
}

static inline gboolean mediaplayer_property_dbus2json(json_object *jprop,
		const gchar *key, GVariant *var, gboolean *is_config,
		GError **error)
{
	return bluez_property_dbus2json(BLUEZ_AT_MEDIAPLAYER,
			jprop, key, var, is_config, error);
}

static inline gboolean mediatransport_property_dbus2json(json_object *jprop,
		const gchar *key, GVariant *var, gboolean *is_config,
		GError **error)
{
	return bluez_property_dbus2json(BLUEZ_AT_MEDIATRANSPORT,
			jprop, key, var, is_config, error);
}

static inline GVariant *device_call(struct bluetooth_state *ns,
		const char *device, const char *method,
		GVariant *params, GError **error)
{
	return bluez_call(ns, BLUEZ_AT_DEVICE, device,
			method, params, error);
}

static inline GVariant *adapter_call(struct bluetooth_state *ns,
		const char *adapter, const char *method,
		GVariant *params, GError **error)
{
	return bluez_call(ns, BLUEZ_AT_ADAPTER, adapter,
			method, params, error);
}

static inline GVariant *agentmanager_call(struct bluetooth_state *ns,
		const char *method, GVariant *params, GError **error)
{
	return bluez_call(ns, BLUEZ_AT_AGENTMANAGER, NULL,
			method, params, error);
}

static inline GVariant *mediaplayer_call(struct bluetooth_state *ns,
		const char *player, const char *method,
		GVariant *params, GError **error)
{
	return bluez_call(ns, BLUEZ_AT_MEDIAPLAYER, player,
			method, params, error);
}

static inline gboolean adapter_set_property(struct bluetooth_state *ns,
		const char *adapter, gboolean is_json_name, const char *name,
		json_object *jval, GError **error)
{
	return bluez_set_property(ns, BLUEZ_AT_ADAPTER, adapter,
				    is_json_name, name, jval, error);
}

static inline json_object *adapter_get_property(struct bluetooth_state *ns,
		gboolean is_json_name, const char *name, GError **error)
{
	return bluez_get_property(ns, BLUEZ_AT_ADAPTER, NULL,
			is_json_name, name, error);
}

static inline json_object *adapter_properties(struct bluetooth_state *ns,
		GError **error, const gchar *adapter)
{
	return bluez_get_properties(ns,
			BLUEZ_AT_ADAPTER, adapter, error);
}

static inline json_object *mediaplayer_properties(struct bluetooth_state *ns,
		GError **error, const gchar *player)
{
	return bluez_get_properties(ns,
			BLUEZ_AT_MEDIAPLAYER, player, error);
}

static inline json_object *mediatransport_properties(struct bluetooth_state *ns,
		GError **error, const gchar *player)
{
	return bluez_get_properties(ns,
			BLUEZ_AT_MEDIATRANSPORT, player, error);
}

static inline json_object *object_properties(struct bluetooth_state *ns,
		GError **error)
{
	return bluez_get_properties(ns,
			BLUEZ_AT_OBJECT, BLUEZ_OBJECT_PATH, error);
}

struct bluez_pending_work {
	struct bluetooth_state *ns;
	void *user_data;
	GCancellable *cancel;
	void (*callback)(void *user_data, GVariant *result, GError **error);
};

void bluez_cancel_call(struct bluetooth_state *ns,
		struct bluez_pending_work *cpw);

struct bluez_pending_work *
bluez_call_async(struct bluetooth_state *ns,
		const char *access_type, const char *type_arg,
		const char *method, GVariant *params, GError **error,
		void (*callback)(void *user_data, GVariant *result, GError **error),
		void *user_data);

void bluez_decode_call_error(struct bluetooth_state *ns,
		const char *access_type, const char *type_arg,
		const char *method,
		GError **error);



#endif /* BLUETOOTH_API_H */
