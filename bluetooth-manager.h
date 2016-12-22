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


#ifndef BLUETOOTH_MANAGER_H
#define BLUETOOTH_MANAGER_H

#include <glib.h>
#include <glib-object.h>
//#include <dbus/dbus.h>
#include <glib.h>
#include <gio/gio.h>
#include <glib-object.h>
//service
#define AGENT_SERVICE               "org.agent"

//remote service
#define BLUEZ_SERVICE               "org.bluez"
#define OFONO_SERVICE               "org.ofono"
#define CLIENT_SERVICE              "org.bluez.obex"

//object path
#define OFONO_MANAGER_PATH          "/"
#define ADAPTER_PATH                "/org/bluez/hci0"
#define OBEX_CLIENT_PATH            "/org/bluez/obex"
#define AGENT_PATH                  "/org/bluez"

//interface
#define ADAPTER_INTERFACE           "org.bluez.Adapter1"
#define DEVICE_INTERFACE            "org.bluez.Device1"
#define AGENT_MANAGER_INTERFACE     "org.bluez.AgentManager"
#define SERVICE_INTERFACE           "org.bluez.Service"
#define AGENT_INTERFACE             "org.bluez.Agent"

#define CLIENT_INTERFACE            "org.bluez.obex.Client"
#define TRANSFER_INTERFACE          "org.bluez.obex.Transfer"
#define SESSION_INTERFACE           "org.bluez.obex.Session"
#define OBEX_ERROR_INTERFACE        "org.bluez.obex.Error"
#define BLUEZ_ERROR_INTERFACE       "org.bluez.Error"
#define PBAP_INTERFACE              "org.bluez.obex.PhonebookAccess"
#define MAP_INTERFACE               "org.bluez.obex.MessageAccess"
#define MAP_MSG_INTERFACE           "org.bluez.obex.Message"

#define MEDIA_PLAYER_INTERFACE      "org.bluez.MediaPlayer"
#define MEDIA_FOLDER_INTERFACE      "org.bluez.MediaFolder"
#define MEDIA_ITEM_INTERFACE        "org.bluez.MediaItem"
#define MEDIA_TRANSPORT_INTERFACE   "org.bluez.MediaTransport"
#define MEDIA_CONTROL1_INTERFACE   "org.bluez.MediaControl1"


#define OFONO_HANDSFREE_INTERFACE               "org.ofono.Handsfree"
#define OFONO_MANAGER_INTERFACE                 "org.ofono.Manager"
#define OFONO_MODEM_INTERFACE                   "org.ofono.Modem"
#define OFONO_VOICECALL_INTERFACE               "org.ofono.VoiceCall"
#define OFONO_VOICECALL_MANAGER_INTERFACE       "org.ofono.VoiceCallManager"
#define OFONO_NETWORK_REGISTRATION_INTERFACE    "org.ofono.NetworkRegistration"
#define OFONO_NETWORK_OPERATOR_INTERFACE        "org.ofono.NetworkOperator"
#define OFONO_CALL_VOLUME_INTERFACE             "org.ofono.CallVolume"

#define FREEDESKTOP_INTROSPECT      "org.freedesktop.DBus.Introspectable"
#define FREEDESKTOP_PROPERTIES      "org.freedesktop.DBus.Properties"


#define CONVERTER_CONN              (cli.sys_conn)
#define AGENT_CONN                  (cli.agent_conn)
#define OBEX_CONN                   (cli.obex_conn)

#define HOMESCREEN_SERVICE				"org.agl.homescreen"
#define HOMESCREEN_ICON_INTERFACE		"org.agl.statusbar"
#define HOMESCREEN_ICON_PATH			"/StatusBar"
#define HOMESCREEN_BT_ICON_POSITION		1

#define DBUS_REPLY_TIMEOUT (120 * 1000)
#define DBUS_REPLY_TIMEOUT_SHORT (10 * 1000)

//typedef void(*callback)(void);
typedef void(*callback)(int password_rejected_flag);
void register_callback(callback ptr);


typedef struct _client
{
    GDBusConnection *sys_conn;
    GDBusConnection *agent_conn;
    GDBusConnection *obex_conn;
    GMainLoop *clientloop;
//    FILE *fd;
//    int conn_fd;
} Client;

//Bluetooth Device Properties
struct btd_device {
    gchar   *bdaddr;
    gchar   *path;
    gchar   *name;
    gboolean    paired;
    gboolean    trusted;
    gboolean    blocked;
    gboolean    connected;
    gboolean    avconnected;
    gboolean    hfpconnected;
    GSList        *uuids;
};

typedef struct {
    gboolean inited;
    GMutex m;
    GSList * device;
} stBluetoothManage;

enum btStates {INACTIVE, ACTIVE};


int BluetoothManageInit(void);

int adapter_set_powered(gboolean value);
int adapter_get_powered(gboolean *value);
int adapter_set_discoverable(gboolean value);
int adapter_start_discovery();
int adapter_stop_discovery();
int adapter_update_devices();
GSList* adapter_get_devices_list();
int adapter_remove_device(struct btd_device * addr);
int device_pair(struct btd_device * addr);
int device_cancelPairing(struct btd_device * addr);
int device_connect(struct btd_device * addr);
//int device_connectProfile();
int device_disconnect(struct btd_device * addr);
//int device_disconnectProfile();
int device_set_property(struct btd_device * addr, const char *property, const char *value);

int isAVPConnected(struct btd_device *BDdevice);
int isHFPConnected(struct btd_device *BDdevice);

GError* setHMIStatus(enum btStates);


#endif /* BLUETOOTH_MANAGER_H */


/************************************** The End Of File **************************************/  

