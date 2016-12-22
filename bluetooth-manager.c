/*  Copyright 2016 ALPS ELECTRIC CO., LTD.
*
*   Licensed under the Apache License, Version 2.0 (the "License");
*   you may not use this file except in compliance with the License.
*   You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
*   Unless required by applicable law or agreed to in writing, software
*   distributed under the License is distributed on an "AS IS" BASIS,
*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*   See the License for the specific language governing permissions and
*   limitations under the License.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <gio/gio.h>
#include <glib-object.h>

#include "bluetooth-api.h"
#include "bluetooth-manager.h"

Client cli = { 0 };

stBluetoothManage BluetoothManage = { 0 };

/* ------ LOCAL  FUNCTIONS --------- */

/*
 register the agent, and register signal watch
 */
void bt_manage_dbus_init(void) {
	D_PRINTF("\n");

	//InitDBusCommunication();

}
/* ------ PUBLIC PLUGIN FUNCTIONS --------- */

/*
 * Init the Bluetooth Manager
 * Note: bluetooth-api shall do BluetoothManageInit() first before call other APIs.
 */
int BluetoothManageInit() {
	D_PRINTF("\n");

	BluetoothManage.device = NULL;
	g_mutex_init(&(BluetoothManage.m));

	bt_manage_dbus_init();

	return 0;
}

/*
 * Set the Bluez Adapter Property "Powered" value
 * If success return 0, else return -1;
 */
int adapter_set_powered(gboolean powervalue) {
	D_PRINTF("value:%d\n",powervalue);
	GDBusConnection *connection;
	GError *error = NULL;

	GVariant *value;
	gboolean result;

	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (NULL == connection) {
		D_PRINTF("GDBusconnection is NULL\n");
		return -1;
	}

	value = g_dbus_connection_call_sync(connection, BLUEZ_SERVICE, ADAPTER_PATH,
	FREEDESKTOP_PROPERTIES, "Set",
			g_variant_new("(ssv)", ADAPTER_INTERFACE, "Powered",
					g_variant_new("b", powervalue)), NULL,
			G_DBUS_CALL_FLAGS_NONE, DBUS_REPLY_TIMEOUT, NULL, &error);

	if (NULL == value) {
		D_PRINTF ("Error getting object manager client: %s", error->message);
		g_error_free(error);
		return -1;
	}

	g_variant_unref(value);
	return 0;
}

/*
 * Get the Bluez Adapter Property "Powered" value
 * If success return 0, else return -1;
 */
int adapter_get_powered(gboolean *powervalue) {
	D_PRINTF("\n");
	GDBusConnection *connection;
	GError *error = NULL;
	GVariant *value = NULL;
	GVariant *subValue = NULL;

	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (NULL == connection) {
		D_PRINTF("GDBusconnection is NULL\n");
		return -1;
	}
	value = g_dbus_connection_call_sync(connection, BLUEZ_SERVICE, ADAPTER_PATH,
	FREEDESKTOP_PROPERTIES, "Get",
			g_variant_new("(ss)", ADAPTER_INTERFACE, "Powered"), NULL,
			G_DBUS_CALL_FLAGS_NONE, DBUS_REPLY_TIMEOUT, NULL, &error);

	if (NULL == value) {
		D_PRINTF ("Error getting object manager client: %s\n", error->message);
		g_error_free(error);
		return -1;
	}

	g_variant_get(value, "(v)", &subValue);
	g_variant_get(subValue, "b", powervalue);
	g_variant_unref(value);

	D_PRINTF("get ret :%d\n",*powervalue);
	return 0;
}

/*
 * Call the Bluez Adapter Method "StartDiscovery"
 * If success return 0, else return -1;
 */
int adapter_start_discovery() {
	D_PRINTF("\n");
	GDBusConnection *connection = NULL;
	GError *error = NULL;
	GVariant *value = NULL;
	GVariant *subValue = NULL;

	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (NULL == connection) {
		D_PRINTF("GDBusconnection is NULL\n");
		return -1;
	}
	value = g_dbus_connection_call_sync(connection, BLUEZ_SERVICE, ADAPTER_PATH,
	ADAPTER_INTERFACE, "StartDiscovery", NULL, NULL, G_DBUS_CALL_FLAGS_NONE,
			DBUS_REPLY_TIMEOUT, NULL, &error);

	if (NULL == value) {
		D_PRINTF ("Error getting object manager client: %s\n", error->message);
		g_error_free(error);
		return -1;
	}

	g_variant_unref(value);
	return 0;
}

/*
 * Call the Bluez Adapter Method "StopDiscovery"
 * If success return 0, else return -1;
 */
int adapter_stop_discovery() {
	D_PRINTF("\n");
	GDBusConnection *connection = NULL;
	GError *error = NULL;
	GVariant *value = NULL;
	GVariant *subValue = NULL;

	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (NULL == connection) {
		D_PRINTF("GDBusconnection is NULL\n");
		return -1;
	}
	value = g_dbus_connection_call_sync(connection, BLUEZ_SERVICE, ADAPTER_PATH,
	ADAPTER_INTERFACE, "StopDiscovery", NULL, NULL, G_DBUS_CALL_FLAGS_NONE,
			DBUS_REPLY_TIMEOUT, NULL, &error);

	if (NULL == value) {
		D_PRINTF ("Error getting object manager client: %s\n", error->message);
		g_error_free(error);
		return -1;
	}

	g_variant_unref(value);

	return 0;
}

/*
 * Call the Bluez Adapter Method "RemoveDevice"
 * If success return 0, else return -1;
 */
int adapter_remove_device(struct btd_device * addr) {
	D_PRINTF("\n%s\t%s\t%s\n",addr->bdaddr,addr->name,addr->path);

	GDBusConnection *connection;
	GError *error = NULL;
	GVariant *value;

	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (NULL == connection) {
		D_PRINTF("GDBusconnection is NULL\n");
		return -1;
	}

	value = g_dbus_connection_call_sync(connection, BLUEZ_SERVICE, ADAPTER_PATH,
	ADAPTER_INTERFACE, "RemoveDevice", g_variant_new("(o)", addr->path), NULL,
			G_DBUS_CALL_FLAGS_NONE, DBUS_REPLY_TIMEOUT, NULL, &error);

	if (NULL == value) {
		D_PRINTF ("Error getting object manager client: %s", error->message);
		g_error_free(error);
		return -1;
	}

	g_variant_unref(value);
	return 0;

}

/*
 * Get the store device list.
 */
//FIXME: gdevices should be added the lock/unlock
GSList* adapter_get_devices_list() {
	return BluetoothManage.device;
}

void lock_devices_list(void) {
	g_mutex_lock(&(BluetoothManage.m));
}

void unlock_devices_list(void) {
	g_mutex_unlock(&(BluetoothManage.m));
}

/*
 * Update the device list
 * Call <method>GetManagedObjects
 * reply type is "Dict of {Object Path, Dict of {String, Dict of {String, Variant}}}"
 */
#if 0
// Test Function
/* recursively iterate a container */
void iterate_container_recursive (GVariant *container)
{
	GVariantIter iter;
	GVariant *child;

	g_variant_iter_init (&iter, container);
	while ((child = g_variant_iter_next_value (&iter)))
	{
		g_print ("type '%s'\n", g_variant_get_type_string (child));

		if (g_variant_is_container (child))
		iterate_container_recursive (child);

		g_variant_unref (child);
	}
}
#endif

int adapter_update_devices() {
	D_PRINTF("\n");
	GDBusConnection *connection = NULL;
	GError *error = NULL;
	GVariant *value = NULL;
	GSList *newDeviceList = NULL;

	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (NULL == connection) {
		D_PRINTF("GDBusconnection is NULL\n");
		return -1;
	}
	value = g_dbus_connection_call_sync(connection, BLUEZ_SERVICE, "/",
			"org.freedesktop.DBus.ObjectManager", "GetManagedObjects", NULL,
			NULL, G_DBUS_CALL_FLAGS_NONE, DBUS_REPLY_TIMEOUT, NULL, &error);

	if (NULL == value) {
		D_PRINTF ("Error getting object manager client: %s\n", error->message);
		g_error_free(error);
		return -1;
	}

	GVariant *subValue = NULL;
	GVariant *subValue_1 = NULL;
	GVariantIter *subValueIter;

	g_variant_get(value, "(*)", &subValue);

	g_variant_get(subValue, "a*", &subValueIter);
	while (g_variant_iter_loop(subValueIter, "*", &subValue_1)) {

#if 0
		iterate_container_recursive(subValue_1);
#else

//FIXME:Bad solution to get the BT address and name
		GVariantIter dbus_object_iter;
		GVariant *dbusObjecValue;
		GVariant *dbusObjecSubValue;
		gchar *dbusObjecPath;
		struct btd_device *device;

		g_variant_iter_init(&dbus_object_iter, subValue_1);

		//DBus Object
		dbusObjecValue = g_variant_iter_next_value(&dbus_object_iter);

		g_variant_get(dbusObjecValue, "o", &dbusObjecPath);

		//ObjectPath is /org/bluez/hci0/dev_xx_xx_xx_xx_xx_xx
		if ((37 != strlen(dbusObjecPath))
				|| (NULL
						== g_strrstr_len(dbusObjecPath, 19,
								"/org/bluez/hci0/dev"))) {
			g_variant_unref(dbusObjecValue);
			continue;
		}
		device = g_try_malloc0(sizeof(struct btd_device));
		device->path = g_strdup(dbusObjecPath);
		g_variant_unref(dbusObjecValue);
		D_PRINTF("Found new device%s\n",device->path );

		//DBus Interfaces and Method/Properties under Interface
		dbusObjecSubValue = g_variant_iter_next_value(&dbus_object_iter);

		GVariantIter *interfaces_iter;
		GVariant *interfaces_subValue;
		g_variant_get(dbusObjecSubValue, "a*", &interfaces_iter);

		while (g_variant_iter_loop(interfaces_iter, "*", &interfaces_subValue)) {
			// D_PRINTF("\t%s\n",g_variant_get_type_string(interfaces_subValue));

			GVariantIter MethodsSignalProperties_iter;
			GVariant *MethodsSignalProperties_name;
			GVariant *MethodsSignalProperties_value;
			gchar *properties_name;

			g_variant_iter_init(&MethodsSignalProperties_iter,
					interfaces_subValue);

			//DBus Interfaces
			MethodsSignalProperties_name = g_variant_iter_next_value(
					&MethodsSignalProperties_iter);

			g_variant_get(MethodsSignalProperties_name, "s", &properties_name);
			//D_PRINTF("\t%s\n",properties_name);
			g_variant_unref(MethodsSignalProperties_name);

			if (NULL
					== g_strrstr_len(properties_name, 20,
							"org.bluez.Device1")) {
				continue;
			}

			D_PRINTF("\t%s\n",properties_name);

			//DBus XXX
			MethodsSignalProperties_value = g_variant_iter_next_value(
					&MethodsSignalProperties_iter);

			GVariantIter *subValue_iter;
			GVariant *subValueVariant;
			g_variant_get(MethodsSignalProperties_value, "a*", &subValue_iter);

			while (g_variant_iter_loop(subValue_iter, "*", &subValueVariant)) {
				GVariantIter sub_subValue_iter;
				GVariant *sub_subValue_name;
				GVariant *sub_subValue_value;
				gchar *key_1 = NULL;
				gchar *key_2 = NULL;

				g_variant_iter_init(&sub_subValue_iter, subValueVariant);

				//DBus Interfaces
				sub_subValue_name = g_variant_iter_next_value(
						&sub_subValue_iter);

				g_variant_get(sub_subValue_name, "s", &key_1);
				D_PRINTF("\t\t%s\n",key_1);

				//DBus XXX
				sub_subValue_value = g_variant_iter_next_value(
						&sub_subValue_iter);

				GVariant *dbus_value;
				dbus_value = g_variant_get_variant(sub_subValue_value);

				if (g_variant_is_of_type(dbus_value, G_VARIANT_TYPE_STRING)) {
					g_variant_get(dbus_value, "s", &key_2);
					//D_PRINTF("\t\t\t%s\ts%s\n",key_1,key_2);

					if (g_strrstr_len(key_1, 10, "Address")) {
						device->bdaddr = g_strdup(key_2);
					} else if (g_strrstr_len(key_1, 10, "Name")) {
						device->name = g_strdup(key_2);
					}
					g_free(key_2);

				} else if (g_variant_is_of_type(dbus_value,
						G_VARIANT_TYPE_BOOLEAN)) {
					gboolean properties_value;
					g_variant_get(dbus_value, "b", &properties_value);

					if (g_strrstr_len(key_1, 10, "Paired")) {
						device->paired = properties_value;
					} else if (g_strrstr_len(key_1, 10, "Blocked")) {
						device->blocked = properties_value;
					} else if (g_strrstr_len(key_1, 10, "Connected")) {
						device->connected = properties_value;
					} else if (g_strrstr_len(key_1, 10, "Trusted")) {
						device->trusted = properties_value;
					}

				}
				g_variant_unref(sub_subValue_name);
				g_variant_unref(sub_subValue_value);

			}
			g_variant_iter_free(subValue_iter);

			g_variant_unref(MethodsSignalProperties_value);

		}
		g_variant_iter_free(interfaces_iter);

		g_variant_unref(dbusObjecSubValue);

#endif

		newDeviceList = g_slist_append(newDeviceList, device);

	}

	g_variant_iter_free(subValueIter);

	g_variant_unref(value);

	//clean first
	GSList * temp = BluetoothManage.device;
	while (temp) {
		struct btd_device *BDdevice = temp->data;
		temp = temp->next;

		BluetoothManage.device = g_slist_remove_all(BluetoothManage.device,
				BDdevice);
		//D_PRINTF("\n%s\n%s\n",BDdevice->bdaddr,BDdevice->name);
		if (BDdevice->bdaddr) {
			g_free(BDdevice->bdaddr);
		}
		if (BDdevice->name) {
			g_free(BDdevice->name);
		}
		if (BDdevice->path) {
			g_free(BDdevice->path);
		}
		g_free(BDdevice);

	}

	BluetoothManage.device = newDeviceList;

}

/*
 * send pairing command
 * If success return 0, else return -1;
 */
int device_pair(struct btd_device * addr) {
	D_PRINTF("\n%s\n%s\t%s\n",addr->bdaddr,addr->name,addr->path);

	GDBusConnection *connection;
	GError *error = NULL;
	GVariant *value;

	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (NULL == connection) {
		D_PRINTF("GDBusconnection is NULL\n");
		return -1;
	}

	value = g_dbus_connection_call_sync(connection, BLUEZ_SERVICE, addr->path,
			"org.bluez.Device1", "Pair", NULL, NULL, G_DBUS_CALL_FLAGS_NONE,
			DBUS_REPLY_TIMEOUT, NULL, &error);

	if (NULL == value) {
		D_PRINTF ("Error getting object manager client: %s", error->message);
		g_error_free(error);
		return -1;
	}

	g_variant_unref(value);
	return 0;

}

/*
 * send cancel pairing command
 * If success return 0, else return -1;
 */
int device_cancelPairing(struct btd_device * addr) {
	D_PRINTF("\n%s\n%s\t%s\n",addr->bdaddr,addr->name,addr->path);

	GDBusConnection *connection;
	GError *error = NULL;
	GVariant *value;

	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (NULL == connection) {
		D_PRINTF("GDBusconnection is NULL\n");
		return -1;
	}

	value = g_dbus_connection_call_sync(connection, BLUEZ_SERVICE, addr->path,
			"org.bluez.Device1", "CancelPairing", NULL, NULL,
			G_DBUS_CALL_FLAGS_NONE, DBUS_REPLY_TIMEOUT, NULL, &error);

	if (NULL == value) {
		D_PRINTF ("Error getting object manager client: %s", error->message);
		g_error_free(error);
		return -1;
	}

	g_variant_unref(value);
	return 0;

}
/*
 * send connect command
 * If success return 0, else return -1;
 */
int device_connect(struct btd_device * addr) {
	D_PRINTF("\n%s\n%s\t%s\n",addr->bdaddr,addr->name,addr->path);

	GDBusConnection *connection;
	GError *error = NULL;
	GVariant *value;

	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (NULL == connection) {
		D_PRINTF("GDBusconnection is NULL\n");
		return -1;
	}

	value = g_dbus_connection_call_sync(connection, BLUEZ_SERVICE, addr->path,
			"org.bluez.Device1", "Connect", NULL, NULL, G_DBUS_CALL_FLAGS_NONE,
			DBUS_REPLY_TIMEOUT, NULL, &error);

	if (NULL == value) {
		D_PRINTF ("Error getting object manager client: %s", error->message);
		g_error_free(error);
		return -1;
	}

	g_variant_unref(value);
	return 0;

}

/*
 * send disconnect command
 * If success return 0, else return -1;
 */
int device_disconnect(struct btd_device * addr) {
	D_PRINTF("\n%s\n%s\t%s\n",addr->bdaddr,addr->name,addr->path);

	GDBusConnection *connection;
	GError *error = NULL;
	GVariant *value;

	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (NULL == connection) {
		D_PRINTF("GDBusconnection is NULL\n");
		return -1;
	}

	value = g_dbus_connection_call_sync(connection, BLUEZ_SERVICE, addr->path,
			"org.bluez.Device1", "Disconnect", NULL, NULL,
			G_DBUS_CALL_FLAGS_NONE, DBUS_REPLY_TIMEOUT, NULL, &error);

	if (NULL == value) {
		D_PRINTF ("Error getting object manager client: %s", error->message);
		g_error_free(error);
		return -1;
	}

	g_variant_unref(value);
	return 0;

}

/*
 * set remote device property
 * If success return 0, else return -1;
 */
int device_set_property(struct btd_device * addr, const char *property_name,
		const char *property_value) {
	D_PRINTF("\n%s\n%s\t%s\n",addr->bdaddr,addr->name,addr->path);

	GDBusConnection *connection;
	GError *error = NULL;
	GVariant *ret;

	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (NULL == connection) {
		D_PRINTF("GDBusconnection is NULL\n");
		return -1;
	}

	//Only support set "Trusted"
	if (strcmp(property_name, "Trusted")) {
		D_PRINTF("Not support property name\n");
		return -1;
	}

	gboolean value;
	if (atoi(property_value) == 1 || !strcasecmp(property_value, "true")) {
		value = TRUE;
	} else if (atoi(property_value) == 0
			|| !strcasecmp(property_value, "false")) {
		value = FALSE;
	} else {
		D_PRINTF("Not support property value\n");
		return -1;
	}

	ret = g_dbus_connection_call_sync(connection, BLUEZ_SERVICE, addr->path,
	FREEDESKTOP_PROPERTIES, "Set",
			g_variant_new("(ssv)", DEVICE_INTERFACE, property_name,
					g_variant_new("b", value)), NULL, G_DBUS_CALL_FLAGS_NONE,
			DBUS_REPLY_TIMEOUT, NULL, &error);

	if (NULL == ret) {
		D_PRINTF ("Error getting object manager client: %s", error->message);
		g_error_free(error);
		return -1;
	}

	g_variant_unref(ret);
	return 0;
}

int isAVPConnected(struct btd_device *addr) {

	GDBusConnection *connection;

	GError *error = NULL;
	GVariant *value;
	GVariant *variantValue;
	gboolean status;

	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (NULL == connection) {
		D_PRINTF("GDBusconnection is NULL\n");
		return -1;
	}

//    value = g_dbus_connection_call_sync(connection, BLUEZ_SERVICE,  BDdevice->path, FREEDESKTOP_PROPERTIES,
//        "Get", g_variant_new("(ss)",MEDIA_CONTROL1_INTERFACE,"Connected"),
//        NULL, G_DBUS_CALL_FLAGS_NONE, DBUS_REPLY_TIMEOUT, NULL, &error);

	value = g_dbus_connection_call_sync(connection, BLUEZ_SERVICE, addr->path,
	FREEDESKTOP_PROPERTIES, "Get",
			g_variant_new("(ss)", MEDIA_CONTROL1_INTERFACE, "Connected"), NULL,
			G_DBUS_CALL_FLAGS_NONE, DBUS_REPLY_TIMEOUT, NULL, &error);

	if (NULL == value) {
		D_PRINTF ("Error getting object manager client: %s\n", error->message);

		g_error_free(error);
		return -1;
	}

	else {
		g_variant_get(value, "(v)", &variantValue);
		g_variant_get(variantValue, "b", &status);
		printf("Address: %s:%i",addr->bdaddr, status);
		return status;
	}

	return 0;

}

int isHFPConnected(struct btd_device *addr) {

	GDBusConnection *connection;

	GError *error = NULL;
	GVariant *value;
	gchar *ofono_path;
	gboolean status;

	GVariantIter *array;
	GVariant *var = NULL;
	const gchar *key = NULL;

	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (NULL == connection) {
		D_PRINTF("GDBusconnection is NULL\n");
		return -1;
	}


//path=/hfp/org/bluez/hci0/dev_E0_98_61_7D_D3_1E; interface=org.ofono.Modem; member=GetProperties

	ofono_path = g_strconcat("/hfp", addr->path, NULL);


	value = g_dbus_connection_call_sync(connection, OFONO_SERVICE, ofono_path,
	OFONO_MODEM_INTERFACE, "GetProperties", NULL, NULL, G_DBUS_CALL_FLAGS_NONE,
			DBUS_REPLY_TIMEOUT, NULL, &error);

	if (NULL == value) {
		D_PRINTF ("Error getting object manager client: %s\n", error->message);

		g_error_free(error);
		return -1;
	}

	else {

		g_variant_get(value, "(a{sv})", &array);
		while (g_variant_iter_loop(array, "{sv}", &key, &var)) {
			if (g_strcmp0(key, "Powered") == 0) {

				g_variant_get(var, "b", &status);

				return status;

			}
		}
		g_variant_iter_free(array);
		g_variant_unref(value);
		g_free(ofono_path);

		return status;
	}

	return 0;

}

GError* setHMIStatus(enum btStates state) {

    gchar *iconString = NULL;
    GDBusConnection *connection;
    GVariant *params = NULL;
    GVariant *message = NULL;
    GError *error = NULL;

    if (state==INACTIVE) iconString = "qrc:/images/Status/HMI_Status_Bluetooth_Inactive-01.png";
    else if (state==ACTIVE) iconString = "qrc:/images/Status/HMI_Status_Bluetooth_On-01.png";
    else iconString = "qrc:/images/Status/HMI_Status_Bluetooth_Inactive-01.png";

    connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

    params = g_variant_new("(is)", HOMESCREEN_BT_ICON_POSITION, iconString);

    message = g_dbus_connection_call_sync(connection, HOMESCREEN_SERVICE,
    HOMESCREEN_ICON_PATH, HOMESCREEN_ICON_INTERFACE, "setStatusIcon", params,
            NULL, G_DBUS_CALL_FLAGS_NONE,
            DBUS_REPLY_TIMEOUT, NULL, &error);

    if (error) {
        printf("error: %s\n", error->message);

        return error;
    } else {
        return NULL;
    }

}

/************************************** The End Of File **************************************/

