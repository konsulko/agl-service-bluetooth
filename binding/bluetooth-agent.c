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

/* Introspection data for the agent service */
static const gchar introspection_xml[] =
"<node>"
"   <interface name='org.bluez.Agent1'>"
"      <method name='RequestConfirmation'>"
"          <arg name='device' direction='in' type='o'/>"
"          <arg name='passkey' direction='in' type='u'/>"
"      </method>"
"      <method name='AuthorizeService'>"
"          <arg name='device' direction='in' type='o'/>"
"          <arg name='uuid' direction='in' type='s'/>"
"      </method>"
"      <method name='Cancel'>"
"      </method>"
"   </interface>"
"</node>";

static void handle_method_call(
		GDBusConnection *connection,
		const gchar *sender_name,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *method_name,
		GVariant *parameters,
		GDBusMethodInvocation *invocation,
		gpointer user_data)
{
	struct bluetooth_state *ns = user_data;
	struct call_work *cw;
	GError *error = NULL;
	json_object *jev = NULL;
	const gchar *path = NULL;

	/* AFB_INFO("sender=%s", sender_name);
	AFB_INFO("object_path=%s", object_path);
	AFB_INFO("interface=%s", interface_name);
	AFB_INFO("method=%s", method_name); */

	if (!g_strcmp0(method_name, "RequestConfirmation")) {
		int pin;

		g_variant_get(parameters, "(ou)", &path, &pin);

		call_work_lock(ns);

		/* can only occur while a pair is issued */
		cw = call_work_lookup_unlocked(ns, "device", NULL, "RequestConfirmation");

		/* if nothing is pending return an error */
		/* TODO: allow client side pairing */
		if (!cw)
			cw = call_work_create_unlocked(ns, "device", NULL,
				"RequestConfirmation", NULL, &error);

		if (!cw) {
			call_work_unlock(ns);
			g_dbus_method_invocation_return_dbus_error(invocation,
					"org.bluez.Error.Rejected",
					"No connection pending");
			return;
		}

		jev = json_object_new_object();
		json_object_object_add(jev, "action", json_object_new_string("request_confirmation"));

		json_process_path(jev, path);
		json_object_object_add(jev, "pincode", json_object_new_int(pin));

		cw->agent_data.pin_code = pin;
		cw->agent_data.device_path = g_strdup(path);
		cw->invocation = invocation;

		call_work_unlock(ns);

		afb_event_push(ns->agent_event, jev);

		return;
	} else if (!g_strcmp0(method_name, "AuthorizeService")) {

		/* g_variant_get(parameters, "(os)", &path, &service);

		jev = json_object_new_object();
		json_object_object_add(jev, "action", json_object_new_string("authorize_service"));
		json_object_object_add(jev, "path", json_object_new_string(path));
		json_object_object_add(jev, "uuid", json_object_new_string(service));

		afb_event_push(ns->agent_event, jev); */

		return g_dbus_method_invocation_return_value(invocation, NULL);
	} else if (!g_strcmp0(method_name, "Cancel")) {
		
		call_work_lock(ns);

		/* can only occur while a pair is issued */
		cw = call_work_lookup_unlocked(ns, "device", NULL, "RequestConfirmation");

		if (!cw) {
			call_work_unlock(ns);
			g_dbus_method_invocation_return_dbus_error(invocation,
					"org.bluez.Error.Rejected",
					"No connection pending");
			return;
		}

		jev = json_object_new_object();
		json_object_object_add(jev, "action", json_object_new_string("canceled_pairing"));

		afb_event_push(ns->agent_event, jev);

		call_work_destroy_unlocked(cw);
		call_work_unlock(ns);

		return g_dbus_method_invocation_return_value(invocation, NULL);
	}

	g_dbus_method_invocation_return_dbus_error(invocation,
			"org.freedesktop.DBus.Error.UnknownMethod",
			"Uknown method");
}

static const GDBusInterfaceVTable interface_vtable = {
	.method_call  = handle_method_call,
	.get_property = NULL,
	.set_property = NULL,
};

static void on_bus_acquired(GDBusConnection *connection,
			    const gchar *name, gpointer user_data)
{
	struct init_data *id = user_data;
	struct bluetooth_state *ns = id->ns;
	GVariant *result;
	GError *error = NULL;

	AFB_INFO("agent bus acquired - registering %s", ns->agent_path);

	ns->registration_id = g_dbus_connection_register_object(connection,
			ns->agent_path,
			ns->introspection_data->interfaces[0],
			&interface_vtable,
			ns,	/* user data */
			NULL,	/* user_data_free_func */
			NULL);

	if (!ns->registration_id) {
		AFB_ERROR("failed to register agent to dbus");
		goto err_unable_to_register_bus;

	}

	result = agentmanager_call(ns, "RegisterAgent",
			g_variant_new("(os)", ns->agent_path, "KeyboardDisplay"),
			&error);
	if (!result) {
		AFB_ERROR("failed to register agent to bluez");
		goto err_unable_to_register_bluez;
	}
	g_variant_unref(result);

	result = agentmanager_call(ns, "RequestDefaultAgent",
			g_variant_new("(o)", ns->agent_path),
			&error);
	if (!result) {
		AFB_ERROR("failed to request default agent to bluez");
		goto err_unable_to_request_default_agent_bluez;
	}
	g_variant_unref(result);

	ns->agent_registered = TRUE;

	AFB_INFO("agent registered at %s", ns->agent_path);
	signal_init_done(id, 0);

	return;

err_unable_to_request_default_agent_bluez:
	agentmanager_call(ns, "UnregisterAgent",
			g_variant_new("(o)", ns->agent_path),
			&error);

err_unable_to_register_bluez:
	g_dbus_connection_unregister_object(ns->conn, ns->registration_id);
	ns->registration_id = 0;

err_unable_to_register_bus:
	signal_init_done(id, -1);
}

int bluetooth_register_agent(struct init_data *id)
{
	struct bluetooth_state *ns = id->ns;

	ns->agent_path = g_strdup_printf("%s/agent%d",
			BLUEZ_PATH, getpid());
	if (!ns->agent_path) {
		AFB_ERROR("can't create agent path");
		goto out_no_agent_path;
	}

	ns->introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, NULL);
	if (!ns->introspection_data) {
		AFB_ERROR("can't create introspection data");
		goto out_no_introspection_data;
	}

	ns->agent_id = g_bus_own_name(G_BUS_TYPE_SYSTEM, BLUEZ_AGENT_INTERFACE,
			G_BUS_NAME_OWNER_FLAGS_REPLACE |
			  G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
			on_bus_acquired,
			NULL,
			NULL,
			id,
			NULL);
	if (!ns->agent_id) {
		AFB_ERROR("can't create agent bus instance");
		goto out_no_bus_name;
	}

	return 0;

out_no_bus_name:
	g_dbus_node_info_unref(ns->introspection_data);
out_no_introspection_data:
	g_free(ns->agent_path);
out_no_agent_path:
	return -1;
}

void bluetooth_unregister_agent(struct bluetooth_state *ns)
{
	g_bus_unown_name(ns->agent_id);
	g_dbus_node_info_unref(ns->introspection_data);
	g_free(ns->agent_path);
}
