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

/**
 * The global thread
 */
static GThread *global_thread;

struct bluetooth_state *bluetooth_get_userdata(afb_req_t request) {
	afb_api_t api = afb_req_get_api(request);
	return afb_api_get_userdata(api);
}

void call_work_lock(struct bluetooth_state *ns)
{
	g_mutex_lock(&ns->cw_mutex);
}

void call_work_unlock(struct bluetooth_state *ns)
{
	g_mutex_unlock(&ns->cw_mutex);
}

static void mediaplayer1_set_path(struct bluetooth_state *ns, const char *path)
{
	if (ns->mediaplayer_path)
		g_free(ns->mediaplayer_path);
	ns->mediaplayer_path = g_strdup(path);
}

static void mediaplayer1_connect_disconnect(struct bluetooth_state *ns,
				            const gchar *player, int state)
{
	GVariant *reply;
	gchar *path = g_strdup(player);
	const char *uuids[] = { "0000110a-0000-1000-8000-00805f9b34fb", "0000110e-0000-1000-8000-00805f9b34fb", NULL };
	const char **tmp = (const char **) uuids;

	*g_strrstr(path, "/") = '\0';

	for (; *tmp; tmp++) {
		reply = bluez_call(ns, "device", path, state ? "ConnectProfile" : "DisconnectProfile", g_variant_new("(&s)", *tmp), NULL);
		if (!reply)
			break;
	}

	g_free(path);
}

struct call_work *call_work_lookup_unlocked(
		struct bluetooth_state *ns,
		const char *access_type, const char *type_arg,
		const char *method)
{
	struct call_work *cw;
	GSList *list;

	/* we can only allow a single pending call */
	for (list = ns->cw_pending; list; list = g_slist_next(list)) {
		cw = list->data;
		if (!g_strcmp0(access_type, cw->access_type) &&
		    !g_strcmp0(type_arg, cw->type_arg) &&
		    !g_strcmp0(method, cw->method))
			return cw;
	}
	return NULL;
}

struct call_work *call_work_lookup(
		struct bluetooth_state *ns,
		const char *access_type, const char *type_arg,
		const char *method)
{
	struct call_work *cw;

	g_mutex_lock(&ns->cw_mutex);
	cw = call_work_lookup_unlocked(ns, access_type, type_arg, method);
	g_mutex_unlock(&ns->cw_mutex);

	return cw;
}

int call_work_pending_id(
		struct bluetooth_state *ns,
		const char *access_type, const char *type_arg,
		const char *method)
{
	struct call_work *cw;
	int id = -1;

	g_mutex_lock(&ns->cw_mutex);
	cw = call_work_lookup_unlocked(ns, access_type, type_arg, method);
	if (cw)
		id = cw->id;
	g_mutex_unlock(&ns->cw_mutex);

	return id;
}

struct call_work *call_work_lookup_by_id_unlocked(
		struct bluetooth_state *ns, int id)
{
	struct call_work *cw;
	GSList *list;

	/* we can only allow a single pending call */
	for (list = ns->cw_pending; list; list = g_slist_next(list)) {
		cw = list->data;
		if (cw->id == id)
			return cw;
	}
	return NULL;
}

struct call_work *call_work_lookup_by_id(
		struct bluetooth_state *ns, int id)
{
	struct call_work *cw;

	g_mutex_lock(&ns->cw_mutex);
	cw = call_work_lookup_by_id_unlocked(ns, id);
	g_mutex_unlock(&ns->cw_mutex);

	return cw;
}

struct call_work *call_work_create_unlocked(struct bluetooth_state *ns,
		const char *access_type, const char *type_arg,
		const char *method, const char *bluez_method,
		GError **error)
{

	struct call_work *cw = NULL;

	cw = call_work_lookup_unlocked(ns, access_type, type_arg, method);
	if (cw) {
		g_set_error(error, NB_ERROR, NB_ERROR_CALL_IN_PROGRESS,
				"another call in progress (%s/%s/%s)",
				access_type, type_arg, method);
		return NULL;
	}

	/* no other pending; allocate */
	cw = g_malloc0(sizeof(*cw));
	cw->ns = ns;
	do {
		cw->id = ns->next_cw_id;
		if (++ns->next_cw_id < 0)
			ns->next_cw_id = 1;
	} while (call_work_lookup_by_id_unlocked(ns, cw->id));

	cw->access_type = g_strdup(access_type);
	cw->type_arg = g_strdup(type_arg);
	cw->method = g_strdup(method);
	cw->bluez_method = g_strdup(bluez_method);

	ns->cw_pending = g_slist_prepend(ns->cw_pending, cw);

	return cw;
}

struct call_work *call_work_create(struct bluetooth_state *ns,
		const char *access_type, const char *type_arg,
		const char *method, const char *bluez_method,
		GError **error)
{

	struct call_work *cw;

	g_mutex_lock(&ns->cw_mutex);
	cw = call_work_create_unlocked(ns,
			access_type, type_arg, method, bluez_method,
			error);
	g_mutex_unlock(&ns->cw_mutex);

	return cw;
}

void call_work_destroy_unlocked(struct call_work *cw)
{
	struct bluetooth_state *ns = cw->ns;
	struct call_work *cw2;

	/* verify that it's something we know about */
	cw2 = call_work_lookup_by_id_unlocked(ns, cw->id);
	if (cw2 != cw) {
		AFB_ERROR("Bad call work to destroy");
		return;
	}

	/* remove it */
	ns->cw_pending = g_slist_remove(ns->cw_pending, cw);

	/* agent struct data */
	g_free(cw->agent_data.device_path);

	g_free(cw->access_type);
	g_free(cw->type_arg);
	g_free(cw->method);
	g_free(cw->bluez_method);
}

void call_work_destroy(struct call_work *cw)
{
	struct bluetooth_state *ns = cw->ns;

	g_mutex_lock(&ns->cw_mutex);
	call_work_destroy_unlocked(cw);
	g_mutex_unlock(&ns->cw_mutex);
}

static afb_event_t get_event_from_value(struct bluetooth_state *ns,
			const char *value)
{
	if (!g_strcmp0(value, "device_changes"))
		return ns->device_changes_event;

	if (!g_strcmp0(value, "media"))
		return ns->media_event;

	if (!g_strcmp0(value, "agent"))
		return ns->agent_event;

	return NULL;
}

static void bluez_devices_signal_callback(
	GDBusConnection *connection,
	const gchar *sender_name,
	const gchar *object_path,
	const gchar *interface_name,
	const gchar *signal_name,
	GVariant *parameters,
	gpointer user_data)
{
	struct bluetooth_state *ns = user_data;
	GVariantIter *array1 = NULL;
	GError *error = NULL;
	GVariant *var = NULL;
	const gchar *path = NULL;
	const gchar *key = NULL;
	json_object *jresp = NULL, *jobj;
	GVariantIter *array = NULL;
	gboolean is_config, ret;
	afb_event_t event = ns->device_changes_event;

	/* AFB_INFO("sender=%s", sender_name);
	AFB_INFO("object_path=%s", object_path);
	AFB_INFO("interface=%s", interface_name);
	AFB_INFO("signal=%s", signal_name); */

	if (!g_strcmp0(signal_name, "InterfacesAdded")) {

		g_variant_get(parameters, "(&oa{sa{sv}})", &path, &array);

		jresp = json_object_new_object();

                json_process_path(jresp, path);

		jobj = json_object_new_object();

		while (g_variant_iter_next(array, "{&s@a{sv}}", &key, &var)) {
			const char *name = NULL;
			GVariant *val = NULL;

			if (g_strcmp0(key, BLUEZ_DEVICE_INTERFACE) &&
			    g_strcmp0(key, BLUEZ_MEDIATRANSPORT_INTERFACE))
				continue;

			array1 = g_variant_iter_new(var);

			while (g_variant_iter_next(array1, "{&sv}", &name, &val)) {
				if (!g_strcmp0(key, BLUEZ_DEVICE_INTERFACE))
					ret = device_property_dbus2json(jobj,
						name, val, &is_config, &error);
				else
					ret = mediatransport_property_dbus2json(jobj,
						name, val, &is_config, &error);
				g_variant_unref(val);
				if (!ret) {
					AFB_DEBUG("%s property %s - %s",
							path,
							key, error->message);
					g_clear_error(&error);
				}
			}
			g_variant_iter_free(array1);
		}
		g_variant_iter_free(array);

		if (array1) {
			json_object_object_add(jresp, "action",
				json_object_new_string("added"));

			if (is_mediatransport1_interface(path)) {
				gchar *endpoint = find_index(path, 5);
				json_object_object_add(jresp, "type",
					json_object_new_string("transport"));
				json_object_object_add(jresp, "endpoint",
					json_object_new_string(endpoint));
				g_free(endpoint);

				event = ns->media_event;
			}
			json_object_object_add(jresp, "properties", jobj);
		} else if (is_mediaplayer1_interface(path) &&
				g_str_has_suffix(path, BLUEZ_DEFAULT_PLAYER)) {

			json_object_object_add(jresp, "connected",
				json_object_new_boolean(TRUE));
			json_object_object_add(jresp, "type",
				json_object_new_string("playback"));
			mediaplayer1_set_path(ns, path);
			event = ns->media_event;
		} else {
			json_object_put(jresp);
			jresp = NULL;
		}

	} else if (!g_strcmp0(signal_name, "InterfacesRemoved")) {

		g_variant_get(parameters, "(&oas)", &path, &array);
		g_variant_iter_free(array);

		jresp = json_object_new_object();
		json_process_path(jresp, path);

		if (is_mediatransport1_interface(path)) {
			gchar *endpoint = find_index(path, 5);
			json_object_object_add(jresp, "type",
				json_object_new_string("transport"));
			json_object_object_add(jresp, "action",
				json_object_new_string("removed"));
			json_object_object_add(jresp, "endpoint",
				json_object_new_string(endpoint));
			g_free(endpoint);

			event = ns->media_event;
		} else if (is_mediaplayer1_interface(path)) {
			json_object_object_add(jresp, "connected",
				json_object_new_boolean(FALSE));
			json_object_object_add(jresp, "type",
				json_object_new_string("playback"));
			event = ns->media_event;
		} else if (split_length(path) == 5) {
			json_object_object_add(jresp, "action",
				json_object_new_string("removed"));
		} else {
			json_object_put(jresp);
			jresp = NULL;
		}

	} else if (!g_strcmp0(signal_name, "PropertiesChanged")) {

		g_variant_get(parameters, "(&sa{sv}as)", &path, &array, &array1);

		if (!g_strcmp0(path, BLUEZ_DEVICE_INTERFACE)) {
			int cnt = 0;

			jresp = json_object_new_object();

			json_process_path(jresp, object_path);
			json_object_object_add(jresp, "action",
					json_object_new_string("changed"));

			jobj = json_object_new_object();

			while (g_variant_iter_next(array, "{&sv}", &key, &var)) {
				ret = device_property_dbus2json(jobj,
						key, var, &is_config, &error);
				g_variant_unref(var);
				if (!ret) {
					AFB_DEBUG("%s property %s - %s",
							"devices",
							key, error->message);
					g_clear_error(&error);
					continue;
				}
				cnt++;
			}

			// NOTE: Possible to get a changed property for something we don't care about
			if (cnt > 0) {
				json_object_object_add(jresp, "properties", jobj);
			} else {
				json_object_put(jobj);
				json_object_put(jresp);
				jresp = NULL;
			}

		} else if (!g_strcmp0(path, BLUEZ_MEDIAPLAYER_INTERFACE) ||
			   !g_strcmp0(path, BLUEZ_MEDIATRANSPORT_INTERFACE)) {
			int cnt = 0;
			jresp = json_object_new_object();
			json_process_path(jresp, object_path);

			while (g_variant_iter_next(array, "{&sv}", &key, &var)) {
				if (!g_strcmp0(path, BLUEZ_MEDIAPLAYER_INTERFACE))
					ret = mediaplayer_property_dbus2json(jresp,
						key, var, &is_config, &error);
				else
					ret = mediatransport_property_dbus2json(jresp,
						key, var, &is_config, &error);
				g_variant_unref(var);
				if (!ret) {
					AFB_DEBUG("%s property %s - %s",
							path,
							key, error->message);
					g_clear_error(&error);
					continue;
				}
				cnt++;
			}

			if (!g_strcmp0(path, BLUEZ_MEDIAPLAYER_INTERFACE)) {
				json_object_object_add(jresp, "type",
					json_object_new_string("playback"));
			} else {
				gchar *endpoint = find_index(object_path, 5);
				json_object_object_add(jresp, "action",
					json_object_new_string("changed"));
				json_object_object_add(jresp, "type",
					json_object_new_string("transport"));
				json_object_object_add(jresp, "endpoint",
					json_object_new_string(endpoint));
				g_free(endpoint);
			}

			// NOTE: Possible to get a changed property for something we don't care about
			if (!cnt) {
				json_object_put(jresp);
				jresp = NULL;
			}

			event = ns->media_event;
		}

		g_variant_iter_free(array);
		g_variant_iter_free(array1);
	}

	if (jresp) {
		afb_event_push(event, jresp);
		jresp = NULL;
	}

	json_object_put(jresp);
}

static struct bluetooth_state *bluetooth_init(GMainLoop *loop)
{
	struct bluetooth_state *ns;
	GError *error = NULL;

	ns = g_try_malloc0(sizeof(*ns));
	if (!ns) {
		AFB_ERROR("out of memory allocating bluetooth state");
		goto err_no_ns;
	}

	AFB_INFO("connecting to dbus");

	ns->loop = loop;
	ns->conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (!ns->conn) {
		if (error)
			g_dbus_error_strip_remote_error(error);
		AFB_ERROR("Cannot connect to D-Bus, %s",
				error ? error->message : "unspecified");
		g_error_free(error);
		goto err_no_conn;

	}

	AFB_INFO("connected to dbus");

	ns->device_changes_event =
		afb_daemon_make_event("device_changes");
	ns->media_event =
		afb_daemon_make_event("media");
	ns->agent_event =
		afb_daemon_make_event("agent");

	if (!afb_event_is_valid(ns->device_changes_event) ||
	    !afb_event_is_valid(ns->media_event) ||
	    !afb_event_is_valid(ns->agent_event)) {
		AFB_ERROR("Cannot create events");
		goto err_no_events;
	}

	ns->device_sub = g_dbus_connection_signal_subscribe(
			ns->conn,
			BLUEZ_SERVICE,
			NULL,   /* interface */
			NULL,	/* member */
			NULL,	/* object path */
			NULL,	/* arg0 */
			G_DBUS_SIGNAL_FLAGS_NONE,
			bluez_devices_signal_callback,
			ns,
			NULL);
	if (!ns->device_sub) {
		AFB_ERROR("Unable to subscribe to interface signals");
		goto err_no_device_sub;
	}

	g_mutex_init(&ns->cw_mutex);
	ns->next_cw_id = 1;

	g_timeout_add_seconds(5, bluetooth_autoconnect, ns);

	return ns;

err_no_device_sub:
	/* no way to clear the events */
err_no_events:
	g_dbus_connection_close(ns->conn, NULL, NULL, NULL);
err_no_conn:
	g_free(ns);
err_no_ns:
	return NULL;
}

static void bluetooth_cleanup(struct bluetooth_state *ns)
{
	g_dbus_connection_signal_unsubscribe(ns->conn, ns->device_sub);
	g_dbus_connection_close(ns->conn, NULL, NULL, NULL);
	g_free(ns);
}

static gpointer bluetooth_func(gpointer ptr)
{
	struct init_data *id = ptr;
	struct bluetooth_state *ns;
	GMainLoop *loop;
	int rc = 0;

	loop = g_main_loop_new(NULL, FALSE);
	if (!loop) {
		AFB_ERROR("Unable to create main loop");
		goto err_no_loop;
	}

	/* real bluetooth init */
	ns = bluetooth_init(loop);
	if (!ns) {
		AFB_ERROR("bluetooth_init() failed");
		goto err_no_ns;
	}

	id->ns = ns;
	rc = bluetooth_register_agent(id);
	if (rc) {
		AFB_ERROR("bluetooth_register_agent() failed");
		goto err_no_agent;
	}

	/* note that we wait for agent registration to signal done */

	afb_api_set_userdata(id->api, ns);
	g_main_loop_run(loop);

	g_main_loop_unref(ns->loop);

	bluetooth_unregister_agent(ns);

	bluetooth_cleanup(ns);
	afb_api_set_userdata(id->api, NULL);

	return NULL;

err_no_agent:
	bluetooth_cleanup(ns);

err_no_ns:
	g_main_loop_unref(loop);

err_no_loop:
	signal_init_done(id, -1);

	return NULL;
}

static int init(afb_api_t api)
{
	struct init_data init_data, *id = &init_data;
	json_object *args = NULL;
	gint64 end_time;
	int ret;

	memset(id, 0, sizeof(*id));
	id->init_done = FALSE;
	id->rc = 0;
	id->api = api;
	g_cond_init(&id->cond);
	g_mutex_init(&id->mutex);

	ret = afb_daemon_require_api("persistence", 1);
	if (ret < 0) {
		AFB_ERROR("Cannot request data persistence service");
		return ret;
	}

	ret = afb_daemon_require_api("network-manager", 1);
	if (ret < 0) {
		AFB_ERROR("Cannot request network manager service");
		return ret;
	}

	args = json_object_new_object();
	json_object_object_add(args , "technology", json_object_new_string("bluetooth"));
	afb_api_call_sync(api, "network-manager", "enable_technology", args, NULL, NULL, NULL);

	global_thread = g_thread_new("agl-service-bluetooth",
				bluetooth_func,
				id);

	AFB_INFO("bluetooth-binding waiting for init done");

	/* wait maximum 10 seconds for init done */
	end_time = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;
	g_mutex_lock(&id->mutex);
	while (!id->init_done) {
		if (!g_cond_wait_until(&id->cond, &id->mutex, end_time))
			break;
	}
	g_mutex_unlock(&id->mutex);

	if (!id->init_done) {
		AFB_ERROR("bluetooth-binding init timeout");
		return -1;
	}

	if (id->rc)
		AFB_ERROR("bluetooth-binding init thread returned %d",
				id->rc);
	else
		AFB_INFO("bluetooth-binding operational");

	id->ns->default_adapter = get_default_adapter(id->api);

	return id->rc;
}

static void mediaplayer1_send_event(struct bluetooth_state *ns)
{
	gchar *player = g_strdup(ns->mediaplayer_path);
	json_object *jresp = mediaplayer_properties(ns, NULL, player);

	if (!jresp)
		goto out_err;

	json_process_path(jresp, player);
	json_object_object_add(jresp, "connected",
			json_object_new_boolean(TRUE));

	afb_event_push(ns->media_event, jresp);

out_err:
	g_free(player);
}

static void bluetooth_subscribe_unsubscribe(afb_req_t request,
		gboolean unsub)
{
	struct bluetooth_state *ns = bluetooth_get_userdata(request);
	json_object *jresp = json_object_new_object();
	const char *value;
	afb_event_t event;
	int rc;

	/* if value exists means to set offline mode */
	value = afb_req_value(request, "value");
	if (!value) {
		afb_req_fail_f(request, "failed", "Missing \"value\" event");
		return;
	}

	event = get_event_from_value(ns, value);
	if (!event) {
		afb_req_fail_f(request, "failed", "Bad \"value\" event \"%s\"",
				value);
		return;
	}

	if (!unsub) {
		rc = afb_req_subscribe(request, event);

		if (!g_strcmp0(value, "media"))
			mediaplayer1_send_event(ns);
	} else {
		rc = afb_req_unsubscribe(request, event);
	}
	if (rc != 0) {
		afb_req_fail_f(request, "failed",
					"%s error on \"value\" event \"%s\"",
					!unsub ? "subscribe" : "unsubscribe",
					value);
		return;
	}

	afb_req_success_f(request, jresp, "Bluetooth %s to event \"%s\"",
			!unsub ? "subscribed" : "unsubscribed",
			value);
}

static void bluetooth_subscribe(afb_req_t request)
{
	bluetooth_subscribe_unsubscribe(request, FALSE);
}

static void bluetooth_unsubscribe(afb_req_t request)
{
	bluetooth_subscribe_unsubscribe(request, TRUE);
}

static void bluetooth_list(afb_req_t request)
{
	struct bluetooth_state *ns = bluetooth_get_userdata(request);
	GError *error = NULL;
	json_object *jresp;

	jresp = object_properties(ns, &error);

	afb_req_success(request, jresp, "Bluetooth - managed objects");
}

static void bluetooth_state(afb_req_t request)
{
	struct bluetooth_state *ns = bluetooth_get_userdata(request);
	GError *error = NULL;
	json_object *jresp;
	const char *adapter = afb_req_value(request, "adapter");

	adapter = BLUEZ_ROOT_PATH(adapter ? adapter : ns->default_adapter);

	jresp = adapter_properties(ns, &error, adapter);
	if (!jresp) {
		afb_req_fail_f(request, "failed", "property %s error %s",
				"State", BLUEZ_ERRMSG(error));
		return;
	}

	afb_req_success(request, jresp, "Bluetooth - adapter state");
}

static void bluetooth_adapter(afb_req_t request)
{
	struct bluetooth_state *ns = bluetooth_get_userdata(request);
	GError *error = NULL;
	const char *adapter = afb_req_value(request, "adapter");
	const char *scan, *discoverable, *powered, *filter, *transport;

	adapter = BLUEZ_ROOT_PATH(adapter ? adapter : ns->default_adapter);

	scan = afb_req_value(request, "discovery");
	if (scan) {
		GVariant *reply = adapter_call(ns, adapter, str2boolean(scan) ?
				"StartDiscovery" : "StopDiscovery", NULL, &error);

		if (!reply) {
			afb_req_fail_f(request, "failed",
					"adapter %s method %s error %s",
					scan, "Scan", BLUEZ_ERRMSG(error));
			g_error_free(error);
			return;
		}
		g_variant_unref(reply);
	}

	discoverable = afb_req_value(request, "discoverable");
	if (discoverable) {
		int ret = adapter_set_property(ns, adapter, FALSE, "Discoverable",
				json_object_new_boolean(str2boolean(discoverable)),
				&error);
		if (!ret) {
			afb_req_fail_f(request, "failed",
					"adapter %s set_property %s error %s",
					adapter, "Discoverable", BLUEZ_ERRMSG(error));
			g_error_free(error);
			return;
		}
	}

	powered = afb_req_value(request, "powered");
	if (powered) {
		int ret = adapter_set_property(ns, adapter, FALSE, "Powered",
				json_object_new_boolean(str2boolean(powered)),
				&error);
		json_object *jresp = NULL;

		if (!ret) {
			afb_req_fail_f(request, "failed",
					"adapter %s set_property %s error %s",
					adapter, "Powered", BLUEZ_ERRMSG(error));
			g_error_free(error);
			return;
		}

		jresp = json_object_new_object();

		json_process_path(jresp, adapter);
		json_object_object_add(jresp, "action",
				json_object_new_string("changed"));
		json_object_object_add(jresp, "powered",
				json_object_new_boolean(str2boolean(powered)));

		afb_event_push(ns->device_changes_event, jresp);
	}

	filter = afb_req_value(request, "filter");
	transport = afb_req_value(request, "transport");

	if (filter || transport) {
		GVariantBuilder builder;
		GVariant *flt, *reply;

		g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));

		if (filter) {
			json_object *jobj = json_tokener_parse(filter);
			gchar **uuid = NULL;

			if (json_object_get_type(jobj) != json_type_array) {
				afb_req_fail_f(request, "failed", "invalid discovery filter");
				return;
			}

			uuid = json_array_to_strv(jobj);
			g_variant_builder_add(&builder, "{sv}", "UUIDs",
					      g_variant_new_strv((const gchar * const *) uuid, -1));

			g_strfreev(uuid);
		}

		if (transport) {
			g_variant_builder_add(&builder, "{sv}", "Transport",
					      g_variant_new_string(transport));

		}

		flt = g_variant_builder_end(&builder);

		reply = adapter_call(ns, adapter, "SetDiscoveryFilter",
				     g_variant_new("(@a{sv})", flt), &error);


		if (!reply) {
			afb_req_fail_f(request, "failed",
					"adapter %s SetDiscoveryFilter error %s",
					adapter, BLUEZ_ERRMSG(error));
			g_error_free(error);
			return;
		}
		g_variant_unref(reply);
	}

	bluetooth_state(request);
}

static void bluetooth_default_adapter(afb_req_t request)
{
	struct bluetooth_state *ns = bluetooth_get_userdata(request);
	const char *adapter = afb_req_value(request, "adapter");
	json_object *jresp = json_object_new_object();

	call_work_lock(ns);
	if (adapter) {
		set_default_adapter(afb_req_get_api(request), adapter);

		if (ns->default_adapter)
			g_free(ns->default_adapter);
		ns->default_adapter = g_strdup(adapter);
	}

	json_object_object_add(jresp, "adapter", json_object_new_string(ns->default_adapter));
	call_work_unlock(ns);

	afb_req_success(request, jresp, "Bluetooth - default adapter");
}

static void connect_service_callback(void *user_data,
		GVariant *result, GError **error)
{
	struct call_work *cw = user_data;
	struct bluetooth_state *ns = cw->ns;

	bluez_decode_call_error(ns,
		cw->access_type, cw->type_arg, cw->bluez_method,
		error);

	if (error && *error) {
		afb_req_fail_f(cw->request, "failed", "Connect error: %s",
				(*error)->message);
		goto out_free;
	}

	if (result)
		g_variant_unref(result);

	afb_req_success_f(cw->request, NULL, "Bluetooth - device %s connected",
			cw->type_arg);
out_free:
	afb_req_unref(cw->request);
	call_work_destroy(cw);
}

static void bluetooth_connect_device(afb_req_t request)
{
	struct bluetooth_state *ns = bluetooth_get_userdata(request);
	GError *error = NULL;
	const char *uuid;
	struct call_work *cw;
	gchar *device;

	device = return_bluez_path(request);
	if (!device) {
		afb_req_fail(request, "failed", "No path given");
		return;
	}

	/* optional, connect single profile */
	uuid = afb_req_value(request, "uuid");

	cw = call_work_create(ns, "device", device,
			"connect_service", "Connect", &error);
	if (!cw) {
		afb_req_fail_f(request, "failed", "can't queue work %s",
				error->message);
		g_error_free(error);
		goto out_free;
	}

	cw->request = request;
	afb_req_addref(request);

	if (uuid)
		cw->cpw = bluez_call_async(ns, "device", device,
			"ConnectProfile", g_variant_new("(&s)", uuid), &error,
			connect_service_callback, cw);
	else
		cw->cpw = bluez_call_async(ns, "device", device,
			"Connect", NULL, &error,
			connect_service_callback, cw);

	if (!cw->cpw) {
		afb_req_fail_f(request, "failed", "connection error %s",
				error->message);
		call_work_destroy(cw);
		g_error_free(error);
		/* fall-thru */
	}

out_free:
	g_free(device);

}

static void bluetooth_disconnect_device(afb_req_t request)
{
	struct bluetooth_state *ns = bluetooth_get_userdata(request);
	json_object *jresp;
	GVariant *reply = NULL;
	GError *error = NULL;
	const char *uuid;
	gchar *device;

	device = return_bluez_path(request);
	if (!device) {
		afb_req_fail(request, "failed", "No device given to disconnect");
		return;
	}

	/* optional, disconnect single profile */
	uuid = afb_req_value(request, "uuid");

	if (uuid)
		reply = device_call(ns, device, "DisconnectProfile",
				g_variant_new("(&s)", uuid), &error);
	else
		reply = device_call(ns, device, "Disconnect", NULL, &error);

	if (!reply) {
		afb_req_fail_f(request, "failed", "Disconnect error %s",
				error ? error->message : "unspecified");
		g_free(device);
		g_error_free(error);
		return;
	}

	g_variant_unref(reply);

	jresp = json_object_new_object();
	afb_req_success_f(request, jresp, "Device - Bluetooth %s disconnected",
			device);

	g_free(device);
}

static void pair_service_callback(void *user_data,
		GVariant *result, GError **error)
{
	struct call_work *cw = user_data;
	struct bluetooth_state *ns = cw->ns;

	bluez_decode_call_error(ns,
		cw->access_type, cw->type_arg, cw->bluez_method,
		error);

	if (error && *error) {
		afb_req_fail_f(cw->request, "failed", "Connect error: %s",
				(*error)->message);
		goto out_free;
	}

	if (result)
		g_variant_unref(result);

	afb_req_success_f(cw->request, NULL, "Bluetooth - device %s paired",
			cw->type_arg);
out_free:
	afb_req_unref(cw->request);
	call_work_destroy(cw);
}

static void bluetooth_pair_device(afb_req_t request)
{
	struct bluetooth_state *ns = bluetooth_get_userdata(request);
	GError *error = NULL;
	gchar *device;
	struct call_work *cw;

	device = return_bluez_path(request);
	if (!device) {
		afb_req_fail(request, "failed", "No path given");
		return;
	}

	cw = call_work_create(ns, "device", device,
			"pair_device", "Pair", &error);
	if (!cw) {
		afb_req_fail_f(request, "failed", "can't queue work %s",
				error->message);
		g_error_free(error);
		goto out_free;
	}

	cw->request = request;
	afb_req_addref(request);

	cw->cpw = bluez_call_async(ns, "device", device, "Pair", NULL, &error,
			pair_service_callback, cw);

	if (!cw->cpw) {
		afb_req_fail_f(request, "failed", "connection error %s",
				error->message);
		call_work_destroy(cw);
		g_error_free(error);
		goto out_free;
	}

out_free:
	g_free(device);

}

static void bluetooth_cancel_pairing(afb_req_t request)
{
	struct bluetooth_state *ns = bluetooth_get_userdata(request);
	struct call_work *cw;
	GVariant *reply = NULL;
	GError *error = NULL;
	gchar *device;

	call_work_lock(ns);

	cw = call_work_lookup_unlocked(ns, "device", NULL, "RequestConfirmation");

	if (!cw) {
		call_work_unlock(ns);
		afb_req_fail(request, "failed", "No pairing in progress");
		return;
	}

	device = cw->agent_data.device_path;
	reply = device_call(ns, device, "CancelPairing", NULL, &error);

	if (!reply) {
		call_work_unlock(ns);
		afb_req_fail_f(request, "failed",
				"device %s method %s error %s",
				device, "CancelPairing", error->message);
		return;
	}

	call_work_unlock(ns);

	afb_req_success(request, json_object_new_object(),
			"Bluetooth - pairing canceled");
}

static void bluetooth_confirm_pairing(afb_req_t request)
{
	struct bluetooth_state *ns = bluetooth_get_userdata(request);
	struct call_work *cw;
	int pin = -1;

	const char *value = afb_req_value(request, "pincode");

	if (value)
		pin = (int)strtol(value, NULL, 10);	

	if (!value || !pin) {
		afb_req_fail_f(request, "failed", "No pincode parameter");
		return;
	}

	call_work_lock(ns);

	cw = call_work_lookup_unlocked(ns, "device", NULL, "RequestConfirmation");

	if (!cw) {
		call_work_unlock(ns);
		afb_req_fail(request, "failed", "No pairing in progress");
		return;
	}

	if (pin == cw->agent_data.pin_code) {
		g_dbus_method_invocation_return_value(cw->invocation, NULL);

		afb_req_success(request, json_object_new_object(),
				"Bluetooth - pairing confimed");
	} else {
		g_dbus_method_invocation_return_dbus_error(cw->invocation,
				"org.bluez.Error.Rejected",
				"No connection pending");

		afb_req_fail(request, "failed", "Bluetooth - pairing failed");
	}

	call_work_destroy_unlocked(cw);
	call_work_unlock(ns);
}

static void bluetooth_remove_device(afb_req_t request)
{
	struct bluetooth_state *ns = bluetooth_get_userdata(request);
	GError *error = NULL;
	GVariant *reply;
	json_object *jval = NULL;
	const char *adapter;
	gchar *device;

	device = return_bluez_path(request);
	if (!device) {
		afb_req_fail(request, "failed", "No path given");
		return;
	}

	jval = bluez_get_property(ns, "device", device, FALSE, "Adapter", &error);	

	if (!jval) {
		afb_req_fail_f(request, "failed",
					" adapter not found for device %s error %s",
					device, error->message);

		g_error_free(error);
		goto out_free;
	}

	adapter = json_object_get_string(jval);

	reply = adapter_call(ns, adapter, "RemoveDevice",
			     g_variant_new("(o)", device), &error);

	if (!reply) {
		afb_req_fail_f(request, "failed",
					" device %s method %s error %s",
					device, "RemoveDevice", error->message);
		g_error_free(error);
		goto out_free;
	}
	g_variant_unref(reply);

	afb_req_success_f(request, json_object_new_object(),
			"Bluetooth - device %s removed", adapter);

out_free:
	if (!jval)
		json_object_put(jval);
	g_free(device);

}

static void bluetooth_avrcp_controls(afb_req_t request)
{
	struct bluetooth_state *ns = bluetooth_get_userdata(request);
	const char *action = afb_req_value(request, "action");
	gchar *device, *player;
	GVariant *reply;
	GError *error = NULL;

	if (!action) {
		afb_req_fail(request, "failed", "No action given");
		return;
	}

	device = return_bluez_path(request);
	if (device) {
		/* TODO: handle multiple players per device */
		player = g_strconcat(device, "/", BLUEZ_DEFAULT_PLAYER, NULL);
		g_free(device);
	} else {
		player = g_strdup(ns->mediaplayer_path);
	}

	if (!player) {
		afb_req_fail(request, "failed", "No path given");
		return;
	}

	if (!g_strcmp0(action, "connect") || !g_strcmp0(action, "disconnect")) {
		mediaplayer1_connect_disconnect(ns, player, !!g_strcmp0(action, "disconnect"));
		goto out_success;
	}

	reply = mediaplayer_call(ns, player, action, NULL, &error);

	if (!reply) {
		afb_req_fail_f(request, "failed",
				"mediaplayer %s method %s error %s",
				player, action, BLUEZ_ERRMSG(error));
		g_free(player);
		g_error_free(error);
		return;
	}

out_success:
	g_free(player);
	afb_req_success(request, NULL, "Bluetooth - AVRCP controls");
}

static void bluetooth_version(afb_req_t request)
{
	json_object *jresp = json_object_new_object();

	json_object_object_add(jresp, "version", json_object_new_string("2.0"));

	afb_req_success(request, jresp, "Bluetooth - Binding version");
}

static const struct afb_verb_v3 bluetooth_verbs[] = {
	{
		.verb = "subscribe",
		.session = AFB_SESSION_NONE,
		.callback = bluetooth_subscribe,
		.info = "Subscribe to the event of 'value'",
	}, {
		.verb = "unsubscribe",
		.session = AFB_SESSION_NONE,
		.callback = bluetooth_unsubscribe,
		.info = "Unsubscribe to the event of 'value'",
	}, {
		.verb = "managed_objects",
		.session = AFB_SESSION_NONE,
		.callback = bluetooth_list,
		.info = "Retrieve managed bluetooth devices"
	}, {
		.verb = "adapter_state",
		.session = AFB_SESSION_NONE,
		.callback = bluetooth_adapter,
		.info = "Set adapter mode and retrieve properties"
	}, {
		.verb = "default_adapter",
		.session = AFB_SESSION_NONE,
		.callback = bluetooth_default_adapter,
		.info = "Set default adapter for commands"
	}, {
		.verb = "connect",
		.session = AFB_SESSION_NONE,
		.callback = bluetooth_connect_device,
		.info = "Connect device and/or profile"
	}, {
		.verb = "disconnect",
		.session = AFB_SESSION_NONE,
		.callback = bluetooth_disconnect_device,
		.info = "Disconnect device and/or profile"
	}, {
		.verb = "pair",
		.session = AFB_SESSION_NONE,
		.callback = bluetooth_pair_device,
		.info = "Pair device"
	}, {
		.verb = "cancel_pairing",
		.session = AFB_SESSION_NONE,
		.callback = bluetooth_cancel_pairing,
		.info = "Cancel pairing"
	}, {
		.verb = "confirm_pairing",
		.session = AFB_SESSION_NONE,
		.callback = bluetooth_confirm_pairing,
		.info = "Confirm pairing",
	}, {
		.verb = "remove_device",
		.session = AFB_SESSION_NONE,
		.callback = bluetooth_remove_device,
		.info = "Removed paired device",
	}, {
		.verb = "avrcp_controls",
		.session = AFB_SESSION_NONE,
		.callback = bluetooth_avrcp_controls,
		.info = "AVRCP controls"
	}, {
		.verb = "version",
		.session = AFB_SESSION_NONE,
		.callback = bluetooth_version,
		.info = "Binding API version",
	},
	{ } /* marker for end of the array */
};

/*
 * description of the binding for afb-daemon
 */
const struct afb_binding_v3 afbBindingV3 = {
	.api = "Bluetooth-Manager",
	.specification = "bluetooth manager API",
	.verbs = bluetooth_verbs,
	.init = init,
};
