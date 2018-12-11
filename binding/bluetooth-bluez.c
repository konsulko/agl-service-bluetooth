/*
 * Copyright 2018 Konsulko Group
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

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#include <glib.h>
#include <stdlib.h>
#include <gio/gio.h>
#include <glib-object.h>

#include <json-c/json.h>

#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>

#include "bluetooth-api.h"
#include "bluetooth-common.h"

static const struct property_info adapter_props[] = {
	{ .name = "UUIDs", 		.fmt = "as", },
	{ .name = "Discoverable",	.fmt = "b", },
	{ .name = "Discovering",	.fmt = "b", },
	{ .name = "Pairable",		.fmt = "b", },
	{ .name = "Powered",		.fmt = "b", },
	{ .name = "Address",		.fmt = "s", },
	{ .name = "AddressType",	.fmt = "s", },
	{ .name = "DiscoverableTimeout",.fmt = "u", },
	{ .name = "PairableTimeout",	.fmt = "u", },
	{ },
};

static const struct property_info device_props[] = {
	{ .name = "Address",		.fmt = "s", },
	{ .name = "AddressType",	.fmt = "s", },
	{ .name = "Name",		.fmt = "s", },
	{ .name = "Adapter",		.fmt = "s", },
	{ .name = "Alias",		.fmt = "s", },
	{ .name = "Class",		.fmt = "u", },
	{ .name = "Icon",		.fmt = "s", },
	{ .name = "Modalias",		.fmt = "s", },
	{ .name	= "Paired",		.fmt = "b", },
	{ .name = "Trusted",		.fmt = "b", },
	{ .name = "Blocked",		.fmt = "b", },
	{ .name = "LegacyPairing",	.fmt = "b", },
	{ .name = "TxPower",		.fmt = "n", },
	{ .name = "RSSI",		.fmt = "n", },
	{ .name = "Connected",		.fmt = "b", },
	{ .name = "UUIDs",		.fmt = "as", },
	{ .name = "Adapter",		.fmt = "s", },
	{ },
};

static const struct property_info mediaplayer_props[] = {
	{ .name = "Position",		.fmt = "u", },
	{ .name = "Status",		.fmt = "s", },
	{
		.name	= "Track",
		.fmt	= "{sv}",
		.sub	= (const struct property_info []) {
			{ .name = "Title",	.fmt = "s", },
			{ .name = "Artist",	.fmt = "s", },
			{ .name = "Album",	.fmt = "s", },
			{ .name = "Genre",	.fmt = "s", },
			{ .name = "NumberOfTracks",	.fmt = "u", },
			{ .name = "TrackNumber",	.fmt = "u", },
			{ .name = "Duration",	.fmt = "u", },
			{ },
		},
	},
	{ },
};

static const struct property_info mediatransport_props[] = {
	{ .name = "UUID",	.fmt = "s", },
	{ .name = "State",	.fmt = "s", },
	{ .name = "Delay",	.fmt = "q", },
	{ .name = "Volume",	.fmt = "q", },
	{ },
};

const struct property_info *bluez_get_property_info(
		const char *access_type, GError **error)
{
	const struct property_info *pi = NULL;

	if (!strcmp(access_type, BLUEZ_AT_ADAPTER))
		pi = adapter_props;
	else if (!strcmp(access_type, BLUEZ_AT_DEVICE))
		pi = device_props;
	else if (!strcmp(access_type, BLUEZ_AT_MEDIAPLAYER))
		pi = mediaplayer_props;
	else if (!strcmp(access_type, BLUEZ_AT_MEDIATRANSPORT))
		pi = mediatransport_props;
	else
		g_set_error(error, NB_ERROR, NB_ERROR_ILLEGAL_ARGUMENT,
				"illegal %s argument", access_type);
	return pi;
}

gboolean bluez_property_dbus2json(const char *access_type,
		json_object *jprop, const gchar *key, GVariant *var,
		gboolean *is_config,
		GError **error)
{
	const struct property_info *pi;
	gboolean ret;

	*is_config = FALSE;
	pi = bluez_get_property_info(access_type, error);
	if (!pi)
		return FALSE;

	ret = root_property_dbus2json(jprop, pi, key, var, is_config);
	if (!ret)
		g_set_error(error, NB_ERROR, NB_ERROR_UNKNOWN_PROPERTY,
				"unknown %s property %s",
				access_type, key);

	return ret;
}


void bluez_decode_call_error(struct bluetooth_state *ns,
		const char *access_type, const char *type_arg,
		const char *method,
		GError **error)
{
	if (!error || !*error)
		return;

	if (strstr((*error)->message,
		   "org.freedesktop.DBus.Error.UnknownObject")) {

		if (!strcmp(method, "Set") ||
		    !strcmp(method, "Get") ||
		    !strcmp(method, "GetAll")) {

			g_clear_error(error);
			g_set_error(error, NB_ERROR,
					NB_ERROR_UNKNOWN_PROPERTY,
					"unknown %s property on %s",
					access_type, type_arg);

		} else if (!strcmp(method, "Connect") ||
			   !strcmp(method, "ConnectProfile") ||
			   !strcmp(method, "Disconnect") ||
			   !strcmp(method, "DisconnectProfile") ||
			   !strcmp(method, "Pair") ||
			   !strcmp(method, "Unpair") ||
			   !strcmp(method, "RemoveDevice")) {

			g_clear_error(error);
			g_set_error(error, NB_ERROR,
					NB_ERROR_UNKNOWN_SERVICE,
					"unknown service %s",
					type_arg);

		} else if (!strcmp(method, "StartDiscovery") ||
			   !strcmp(method, "StopDiscovery") ||
			   !strcmp(method, "SetDiscoveryFilter") ||
			   !strcmp(method, "RegisterAgent")) {

			g_clear_error(error);
			g_set_error(error, NB_ERROR,
					NB_ERROR_UNKNOWN_SERVICE,
					"unknown service %s",
					type_arg);
		} else if (!strcmp(method, "Play") ||
			   !strcmp(method, "Pause") ||
			   !strcmp(method, "Stop") ||
			   !strcmp(method, "Next") ||
			   !strcmp(method, "Previous") ||
			   !strcmp(method, "FastForward") ||
			   !strcmp(method, "Rewind")) {

			g_clear_error(error);
			g_set_error(error, NB_ERROR,
					NB_ERROR_UNKNOWN_PROPERTY,
					"unknown method %s",
					method);
		}
	}
}

GVariant *bluez_call(struct bluetooth_state *ns,
		const char *access_type, const char *path,
		const char *method, GVariant *params, GError **error)
{
	const char *interface;
	GVariant *reply;

	if (!path && (!strcmp(access_type, BLUEZ_AT_DEVICE) ||
			  !strcmp(access_type, BLUEZ_AT_ADAPTER) ||
			  !strcmp(access_type, BLUEZ_AT_MEDIAPLAYER))) {
		g_set_error(error, NB_ERROR, NB_ERROR_MISSING_ARGUMENT,
				"missing %s argument",
				access_type);
		return NULL;
	}

	if (!strcmp(access_type, BLUEZ_AT_DEVICE)) {
		interface = BLUEZ_DEVICE_INTERFACE;
	} else if (!strcmp(access_type, BLUEZ_AT_ADAPTER)) {
		interface = BLUEZ_ADAPTER_INTERFACE;
	} else if (!strcmp(access_type, BLUEZ_AT_AGENTMANAGER)) {
		path = BLUEZ_PATH;
		interface = BLUEZ_AGENTMANAGER_INTERFACE;
	} else if (!strcmp(access_type, BLUEZ_AT_MEDIAPLAYER)) {
		interface = BLUEZ_MEDIAPLAYER_INTERFACE;
	} else {
		g_set_error(error, NB_ERROR, NB_ERROR_ILLEGAL_ARGUMENT,
				"illegal %s argument",
				access_type);
		return NULL;
	}

	reply = g_dbus_connection_call_sync(ns->conn,
			BLUEZ_SERVICE, path, interface, method, params,
			NULL, G_DBUS_CALL_FLAGS_NONE, DBUS_REPLY_TIMEOUT,
			NULL, error);
	bluez_decode_call_error(ns, access_type, path, method,
			error);
	if (!reply) {
		if (error && *error)
			g_dbus_error_strip_remote_error(*error);
		AFB_ERROR("Error calling %s%s%s %s method %s",
				access_type,
				path ? "/" : "",
				path ? path : "",
				method,
				error && *error ? (*error)->message :
					"unspecified");
	}

	return reply;
}

static void bluez_call_async_ready(GObject *source_object,
		GAsyncResult *res, gpointer user_data)
{
	struct bluez_pending_work *cpw = user_data;
	struct bluetooth_state *ns = cpw->ns;
	GVariant *result;
	GError *error = NULL;

	result = g_dbus_connection_call_finish(ns->conn, res, &error);

	cpw->callback(cpw->user_data, result, &error);

	g_clear_error(&error);
	g_cancellable_reset(cpw->cancel);
	g_free(cpw);
}

void bluez_cancel_call(struct bluetooth_state *ns,
		struct bluez_pending_work *cpw)
{
	g_cancellable_cancel(cpw->cancel);
}

struct bluez_pending_work *
bluez_call_async(struct bluetooth_state *ns,
		const char *access_type, const char *type_arg,
		const char *method, GVariant *params, GError **error,
		void (*callback)(void *user_data, GVariant *result, GError **error),
		void *user_data)
{
	const char *path;
	const char *interface;
	struct bluez_pending_work *cpw;

	if (!type_arg && (!strcmp(access_type, BLUEZ_AT_DEVICE) ||
			  !strcmp(access_type, BLUEZ_AT_ADAPTER))) {
		g_set_error(error, NB_ERROR, NB_ERROR_MISSING_ARGUMENT,
				"missing %s argument",
				access_type);
		return NULL;
	}

	if (!strcmp(access_type, BLUEZ_AT_DEVICE)) {
		path = type_arg;
		interface = BLUEZ_DEVICE_INTERFACE;
	} else if (!strcmp(access_type, BLUEZ_AT_ADAPTER)) {
		path = type_arg;
		interface = BLUEZ_ADAPTER_INTERFACE;
	} else {
		g_set_error(error, NB_ERROR, NB_ERROR_ILLEGAL_ARGUMENT,
				"illegal %s argument",
				access_type);
		return NULL;
	}

	cpw = g_malloc(sizeof(*cpw));
	if (!cpw) {
		g_set_error(error, NB_ERROR, NB_ERROR_OUT_OF_MEMORY,
				"out of memory");
		return NULL;
	}
	cpw->ns = ns;
	cpw->user_data = user_data;
	cpw->cancel = g_cancellable_new();
	if (!cpw->cancel) {
		g_free(cpw);
		g_set_error(error, NB_ERROR, NB_ERROR_OUT_OF_MEMORY,
				"out of memory");
		return NULL;
	}
	cpw->callback = callback;

	g_dbus_connection_call(ns->conn,
			BLUEZ_SERVICE, path, interface, method, params,
			NULL,	/* reply type */
			G_DBUS_CALL_FLAGS_NONE, DBUS_REPLY_TIMEOUT,
			cpw->cancel,	/* cancellable? */
			bluez_call_async_ready,
			cpw);

	return cpw;
}

json_object *bluez_get_properties(struct bluetooth_state *ns,
		const char *access_type, const char *path,
		GError **error)
{
	const struct property_info *pi = NULL;
	const char *method = NULL;
	GVariant *reply = NULL;
	GVariantIter *array, *array2, *array3;
	GVariant *var = NULL;
	const char *interface, *interface2, *path2 = NULL;
	const gchar *key = NULL;
	json_object *jarray = NULL, *jarray2 = NULL, *jarray3 = NULL;
	json_object *jprop = NULL, *jresp = NULL, *jtype = NULL;
	gboolean is_config;

	if (!strcmp(access_type, BLUEZ_AT_DEVICE) ||
	    !strcmp(access_type, BLUEZ_AT_MEDIAPLAYER) ||
	    !strcmp(access_type, BLUEZ_AT_MEDIATRANSPORT) ||
	    !strcmp(access_type, BLUEZ_AT_ADAPTER)) {

		pi = bluez_get_property_info(access_type, error);
		if (!pi)
			return NULL;

		interface = FREEDESKTOP_PROPERTIES;
		method = "GetAll";
	} else if (!strcmp(access_type, BLUEZ_AT_OBJECT)) {
		interface = FREEDESKTOP_OBJECTMANAGER;
		method = "GetManagedObjects";
	} else {
		return FALSE;
	}

	if (!strcmp(access_type, BLUEZ_AT_DEVICE))
		interface2 = BLUEZ_DEVICE_INTERFACE;
	else if (!strcmp(access_type, BLUEZ_AT_MEDIAPLAYER))
		interface2 = BLUEZ_MEDIAPLAYER_INTERFACE;
	else if (!strcmp(access_type, BLUEZ_AT_MEDIATRANSPORT))
		interface2 = BLUEZ_MEDIATRANSPORT_INTERFACE;
	else if (!strcmp(access_type, BLUEZ_AT_ADAPTER))
		interface2 = BLUEZ_ADAPTER_INTERFACE;
	else if (!strcmp(access_type, BLUEZ_AT_OBJECT))
		interface2 = NULL;
	else
		return FALSE;

	if (!method) {
		g_set_error(error, NB_ERROR, NB_ERROR_ILLEGAL_ARGUMENT,
				"illegal %s argument",
				access_type);
		return NULL;
	}

	reply = g_dbus_connection_call_sync(ns->conn,
			BLUEZ_SERVICE, path, interface, method,
			interface2 ? g_variant_new("(s)", interface2) : NULL,
			NULL, G_DBUS_CALL_FLAGS_NONE, DBUS_REPLY_TIMEOUT,
			NULL, error);

	if (!reply)
		return NULL;

	if (!strcmp(access_type, BLUEZ_AT_DEVICE) ||
	    !strcmp(access_type, BLUEZ_AT_MEDIAPLAYER) ||
	    !strcmp(access_type, BLUEZ_AT_MEDIATRANSPORT) ||
	    !strcmp(access_type, BLUEZ_AT_ADAPTER)) {
		jprop = json_object_new_object();
		g_variant_get(reply, "(a{sv})", &array);
		while (g_variant_iter_loop(array, "{sv}", &key, &var)) {
			root_property_dbus2json(jprop, pi,
					key, var, &is_config);
		}
		g_variant_iter_free(array);
		g_variant_unref(reply);
		jresp = jprop;
	} else if (!strcmp(access_type, BLUEZ_AT_OBJECT)) {
		jarray = json_object_new_array();
		jarray2 = json_object_new_array();
		jarray3 = json_object_new_array();

		jresp = json_object_new_object();
		json_object_object_add(jresp, "adapters", jarray);
		json_object_object_add(jresp, "devices", jarray2);
		json_object_object_add(jresp, "transports", jarray3);

		g_variant_get(reply, "(a{oa{sa{sv}}})", &array);
		while (g_variant_iter_loop(array, "{oa{sa{sv}}}", &path2, &array2)) {

			while (g_variant_iter_loop(array2, "{&sa{sv}}", &interface, &array3)) {
				json_object *array = NULL;
				gchar *tmp = NULL;

				jtype = json_object_new_object();

				if (!strcmp(interface, BLUEZ_ADAPTER_INTERFACE)) {
					access_type = BLUEZ_AT_ADAPTER;
					array = jarray;

					tmp = bluez_return_adapter(path2);
					json_object_object_add(jtype, "name", json_object_new_string(tmp));
					g_free(tmp);
				} else if (!strcmp(interface, BLUEZ_DEVICE_INTERFACE)) {
					access_type = BLUEZ_AT_DEVICE;
					array = jarray2;

					json_process_path(jtype, path2);
				} else if (!strcmp(interface, BLUEZ_MEDIATRANSPORT_INTERFACE)) {
					gchar *endpoint = find_index(path2, 5);

					access_type = BLUEZ_AT_MEDIATRANSPORT;
					array = jarray3;

					json_object_object_add(jtype, "endpoint",
						json_object_new_string(endpoint));
					g_free(endpoint);

					json_process_path(jtype, path2);
			 	} else {
					json_object_put(jtype);
					continue; /* TODO: Maybe display other interfaces */
				}

				pi = bluez_get_property_info(access_type, error);

				while (g_variant_iter_loop(array3, "{sv}", &key, &var)) {
					if (!jprop)
						jprop = json_object_new_object();

					root_property_dbus2json(jprop, pi,
						key, var, &is_config);
				}

				json_object_object_add(jtype, "properties", jprop);
				json_object_array_add(array, jtype);
				jprop = NULL;
			}

		}

		g_variant_iter_free(array);
		g_variant_unref(reply);
	}

	if (!jresp) {
		g_set_error(error, NB_ERROR, NB_ERROR_ILLEGAL_ARGUMENT,
				"No %s", access_type);
	}

	return jresp;
}

json_object *bluez_get_property(struct bluetooth_state *ns,
		const char *access_type, const char *path,
		gboolean is_json_name, const char *name, GError **error)
{
	const struct property_info *pi;
	json_object *jprop, *jval;

	pi = bluez_get_property_info(access_type, error);
	if (!pi)
		return NULL;

	jprop = bluez_get_properties(ns, access_type, path, error);
	if (!jprop)
		return NULL;

	jval = get_named_property(pi, is_json_name, name, jprop);
	json_object_put(jprop);

	if (!jval)
		g_set_error(error, NB_ERROR, NB_ERROR_BAD_PROPERTY,
				"Bad property %s on %s%s%s", name,
				access_type,
				path ? "/" : "",
				path ? path : "");
	return jval;
}

/* NOTE: jval is consumed */
gboolean bluez_set_property(struct bluetooth_state *ns,
		const char *access_type, const char *path,
		gboolean is_json_name, const char *name, json_object *jval,
		GError **error)
{
	const struct property_info *pi;
	GVariant *reply, *arg;
	const char *interface;
	gboolean is_config;
	gchar *propname;

	g_assert(path);

	/* get start of properties */
	pi = bluez_get_property_info(access_type, error);
	if (!pi)
		return FALSE;

	/* get actual property */
	pi = property_by_name(pi, is_json_name, name, &is_config);
	if (!pi) {
		g_set_error(error, NB_ERROR, NB_ERROR_UNKNOWN_PROPERTY,
				"unknown property with name %s", name);
		json_object_put(jval);
		return FALSE;
	}

	/* convert to gvariant */
	arg = property_json_to_gvariant(pi, NULL, NULL, jval, error);

	/* we don't need this anymore */
	json_object_put(jval);
	jval = NULL;

	/* no variant? error */
	if (!arg)
		return FALSE;

	if (!is_config)
		propname = g_strdup(pi->name);
	else
		propname = configuration_dbus_name(pi->name);

	if (!strcmp(access_type, BLUEZ_AT_DEVICE))
		interface = BLUEZ_DEVICE_INTERFACE;
	else if (!strcmp(access_type, BLUEZ_AT_ADAPTER))
		interface = BLUEZ_ADAPTER_INTERFACE;
	else if (!strcmp(access_type, BLUEZ_AT_AGENT))
		interface = BLUEZ_AGENT_INTERFACE;
	else
		return FALSE;

	reply = g_dbus_connection_call_sync(ns->conn,
			BLUEZ_SERVICE, path, FREEDESKTOP_PROPERTIES, "Set",
			g_variant_new("(ssv)", interface, propname, arg),
			NULL, G_DBUS_CALL_FLAGS_NONE, DBUS_REPLY_TIMEOUT,
			NULL, error);

	g_free(propname);

	if (!reply)
		return FALSE;

	g_variant_unref(reply);

	return TRUE;
}

gboolean bluetooth_autoconnect(gpointer data)
{
	struct bluetooth_state *ns = data;
	GError *error = NULL;
	json_object *jresp, *jobj = NULL;
        int i;

	jresp = object_properties(ns, &error);

	json_object_object_get_ex(jresp, "devices", &jobj);

        for (i = 0; i < json_object_array_length(jobj); i++) {
		json_object *idx = json_object_array_get_idx(jobj, i);
		json_object *props = NULL;
		json_object_object_get_ex(idx, "properties", &props);

		if (props) {
			json_object *paired = NULL;
			json_object_object_get_ex(props, "paired", &paired);

			if (paired && json_object_get_boolean(paired)) {
				GVariant *reply;
                                json_object *tmp = NULL;
				const char *adapter, *device;
				gchar *path;

                                json_object_object_get_ex(idx, "device", &tmp);
				device = json_object_get_string(tmp);

                                json_object_object_get_ex(idx, "adapter", &tmp);
				adapter = json_object_get_string(tmp);

				path = g_strconcat("/org/bluez/", adapter, "/", device, NULL);

				reply = bluez_call(ns, "device", path, "Connect", NULL, NULL);
				g_free(path);

				if (!reply)
					continue;
				g_variant_unref(reply);

				return FALSE;
			}
		}
	}

	return FALSE;
}
