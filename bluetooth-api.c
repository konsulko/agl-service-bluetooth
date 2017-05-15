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

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <json-c/json.h>
#include <afb/afb-binding.h>
#include <afb/afb-service-itf.h>

#include "bluetooth-manager.h"
#include "bluetooth-agent.h"
#include "bluetooth-api.h"

/*
 * the interface to afb-daemon
 */
const struct afb_binding_interface *afbitf;

struct event
{
	struct event *next;
	struct afb_event event;
	char tag[1];
};

static struct event *events = 0;

/* searchs the event of tag */
static struct event *event_get(const char *tag)
{
	struct event *e = events;
	while(e && strcmp(e->tag, tag))
		e = e->next;
	return e;
}

/* deletes the event of tag */
static int event_del(const char *tag)
{
	struct event *e, **p;

	/* check exists */
	e = event_get(tag);
	if (!e) return -1;

	/* unlink */
	p = &events;
	while(*p != e) p = &(*p)->next;
	*p = e->next;

	/* destroys */
	afb_event_drop(e->event);
	free(e);
	return 0;
}

/* creates the event of tag */
static int event_add(const char *tag, const char *name)
{
	struct event *e;

	/* check valid tag */
	e = event_get(tag);
	if (e) return -1;

	/* creation */
	e = malloc(strlen(tag) + sizeof *e);
	if (!e) return -1;
	strcpy(e->tag, tag);

	/* make the event */
	e->event = afb_daemon_make_event(afbitf->daemon, name);
	if (!e->event.closure) { free(e); return -1; }

	/* link */
	e->next = events;
	events = e;
	return 0;
}

static int event_subscribe(struct afb_req request, const char *tag)
{
	struct event *e;
	e = event_get(tag);
	return e ? afb_req_subscribe(request, e->event) : -1;
}

static int event_unsubscribe(struct afb_req request, const char *tag)
{
	struct event *e;
	e = event_get(tag);
	return e ? afb_req_unsubscribe(request, e->event) : -1;
}

static int event_push(struct json_object *args, const char *tag)
{
	struct event *e;
	e = event_get(tag);
	return e ? afb_event_push(e->event, json_object_get(args)) : -1;
}

/* create device json object*/
static json_object *new_json_object_from_device(struct btd_device *BDdevice, unsigned int filter)
{
    json_object *jresp = json_object_new_object();
    json_object *jstring = NULL;

    if (BD_PATH & filter)
    {
        if (BDdevice->path)
        {
            jstring = json_object_new_string(BDdevice->path);
        }
        else
        {
            jstring = json_object_new_string("");
        }
        json_object_object_add(jresp, "Path", jstring);
    }

    if (BD_ADDER & filter)
    {
        if (BDdevice->bdaddr)
        {
            jstring = json_object_new_string(BDdevice->bdaddr);
        }
        else
        {
            jstring = json_object_new_string("");
        }
        json_object_object_add(jresp, "Address", jstring);
    }

    if (BD_NAME & filter)
    {
        if (BDdevice->name)
        {
            jstring = json_object_new_string(BDdevice->name);
        }
        else
        {
            jstring = json_object_new_string("");
        }
        json_object_object_add(jresp, "Name", jstring);
    }

    if (BD_PAIRED & filter)
    {
        jstring = (TRUE == BDdevice->paired) ?
            json_object_new_string("True"):json_object_new_string("False");
        json_object_object_add(jresp, "Paired", jstring);
    }

    if (BD_TRUSTED & filter)
    {
        jstring = (TRUE == BDdevice->trusted) ?
            json_object_new_string("True"):json_object_new_string("False");
        json_object_object_add(jresp, "Trusted", jstring);
    }

    if (BD_ACLCONNECTED & filter)
    {
        jstring = (TRUE == BDdevice->connected) ?
            json_object_new_string("True"):json_object_new_string("False");
        json_object_object_add(jresp, "Connected", jstring);
    }

    if (BD_AVCONNECTED & filter)
    {
        jstring = (TRUE == BDdevice->avconnected) ?
            json_object_new_string("True"):json_object_new_string("False");
        json_object_object_add(jresp, "AVPConnected", jstring);
    }

    if (BD_HFPCONNECTED & filter)
    {
        jstring = (TRUE == BDdevice->hfpconnected) ?
            json_object_new_string("True"):json_object_new_string("False");
        json_object_object_add(jresp, "HFPConnected", jstring);
    }

    return jresp;
}

/**/
static void bt_power (struct afb_req request)
{
    LOGD("\n");

    const char *value = afb_req_value (request, "value");
    json_object *jresp = NULL;
    int ret = 0;

    jresp = json_object_new_object();

    /* no "?value=" parameter : return current state */
    if (!value) {
        gboolean power_value;
        ret = adapter_get_powered(&power_value);

        if (0==ret)
        {

            setHMIStatus(ACTIVE);
            (TRUE==power_value)?json_object_object_add (jresp, "power", json_object_new_string ("on"))
                                : json_object_object_add (jresp, "power", json_object_new_string ("off"));
        }
        else
        {
            afb_req_fail (request, "failed", "Unable to get power status");
            return;
        }

    }

    /* "?value=" parameter is "1" or "true" */
    else if ( atoi(value) == 1 || !strcasecmp(value, "true") )
    {
        if (adapter_set_powered (TRUE))
        {
            afb_req_fail (request,"failed","no more radio devices available");
            return;
        }
        json_object_object_add (jresp, "power", json_object_new_string ("on"));
        setHMIStatus(ACTIVE);
    }

    /* "?value=" parameter is "0" or "false" */
    else if ( atoi(value) == 0 || !strcasecmp(value, "false") )
    {
        if (adapter_set_powered (FALSE))
        {
            afb_req_fail (request, "failed", "Unable to release radio device");
            return;
        }

        json_object_object_add (jresp, "power", json_object_new_string("off"));
        setHMIStatus(INACTIVE);
    }
    else
    {
        afb_req_fail (request, "failed", "Invalid value");
        return;
    }

    afb_req_success (request, jresp, "Radio - Power set");
}

/**/
static void bt_start_discovery (struct afb_req request)
{
    LOGD("\n");
    int ret = 0;

    ret = adapter_start_discovery();

    if (ret)
    {
        afb_req_fail (request, "failed", "Unable to start discovery");
        return;
    }

    afb_req_success (request, NULL, NULL);
}

/**/
static void bt_stop_discovery (struct afb_req request)
{
    LOGD("\n");
    int ret = 0;

    ret = adapter_stop_discovery();

    if (ret)
    {
        afb_req_fail (request, "failed", "Unable to stop discovery");
        return;
    }

    afb_req_success (request, NULL, NULL);
}


/**/
static void bt_discovery_result (struct afb_req request)
{
    LOGD("\n");
    GSList *list = NULL;
    GSList *tmp = NULL;
    //adapter_update_devices();
    list = adapter_get_devices_list();
    if (NULL == list)
    {
        afb_req_fail (request, "failed", "No find devices");
        return;
    }

    json_object *my_array = json_object_new_array();

    tmp = list;
    for(;tmp;tmp=tmp->next)
    {
        struct btd_device *BDdevice = tmp->data;
        //LOGD("\n%s\t%s\n",BDdevice->bdaddr,BDdevice->name);

        unsigned int filter = BD_ADDER|BD_NAME|BD_PAIRED|BD_ACLCONNECTED|BD_AVCONNECTED|BD_HFPCONNECTED;

        json_object *jresp = new_json_object_from_device(BDdevice, filter);

        json_object_array_add(my_array, jresp);
    }

    adapter_devices_list_free(list);

    afb_req_success(request, my_array, "BT - Scan Result is Displayed");
}

/**/
static void bt_pair (struct afb_req request)
{
    LOGD("\n");

    const char *value = afb_req_value (request, "value");
    int ret = 0;

    if (NULL == value)
    {
        afb_req_fail (request, "failed", "Please Input the Device Address");
        return;
    }

    ret = device_pair(value);
    if (0 == ret)
    {
        afb_req_success (request, NULL, NULL);
    }
    else
    {
        afb_req_fail (request, "failed", "Device pairing failed");
    }

}

/**/
static void bt_cancel_pairing (struct afb_req request)
{
    LOGD("\n");

    const char *value = afb_req_value (request, "value");
    int ret = 0;

    if (NULL == value)
    {
        afb_req_fail (request, "failed", "Please Input the Device Address");
        return;
    }

    ret = device_cancelPairing(value);
    if (0 == ret)
    {
        afb_req_success (request, NULL, NULL);
    }
    else
    {
        afb_req_fail (request, "failed", "Device cancel pairing failed");
    }

}

/**/
static void bt_connect (struct afb_req request)
{
    LOGD("\n");

    const char *value = afb_req_value (request, "value");
    int ret = 0;

    if (NULL == value)
    {
        afb_req_fail (request, "failed", "Please Input the Device Address");
        return;
    }

    ret = device_connect(value);

    if (0 == ret)
    {
        afb_req_success (request, NULL, NULL);
    }
    else
    {
        afb_req_fail (request, "failed", "Device connect failed");
    }

}

/**/
static void bt_disconnect (struct afb_req request)
{
    LOGD("\n");

    const char *value = afb_req_value (request, "value");
    int ret = 0;

    if (NULL == value)
    {
        afb_req_fail (request, "failed", "Please Input the Device Address");
        return;
    }

    ret = device_disconnect(value);
    if (0 == ret)
    {
        afb_req_success (request, NULL, NULL);
    }
    else
    {
        afb_req_fail (request, "failed", "Device disconnect failed");
    }

}

/**/
static void bt_remove_device (struct afb_req request)
{
    LOGD("\n");

    const char *value = afb_req_value (request, "value");
    int ret = 0;

    if (NULL == value)
    {
        afb_req_fail (request, "failed", "Please Input the Device Address");
        return;
    }

    ret = adapter_remove_device(value);
    if (0 == ret)
    {
                afb_req_success (request, NULL, NULL);
    }
    else
    {
        afb_req_fail (request, "failed", "Remove Device failed");
    }

}


/**/
static void bt_set_device_property (struct afb_req request)
{
    LOGD("\n");

    const char *address = afb_req_value (request, "Address");
    const char *property = afb_req_value (request, "Property");
    const char *value = afb_req_value (request, "value");
    int ret = 0;
    GSList *list = NULL;

    if (NULL == address || NULL==property || NULL==value)
    {
        afb_req_fail (request, "failed", "Please Check Input Parameter");
        return;
    }

    ret = device_set_property(address, property, value);
    if (0 == ret)
    {
        afb_req_success (request, NULL, NULL);
    }
    else
    {
        afb_req_fail (request, "failed", "Device set property failed");
    }

}

/**/
static void bt_set_property (struct afb_req request)
{
    LOGD("\n");

    const char *property = afb_req_value (request, "Property");
    const char *value = afb_req_value (request, "value");
    int ret = 0;
    gboolean setvalue;


    if (NULL==property || NULL==value)
    {
        afb_req_fail (request, "failed", "Please Check Input Parameter");
        return;
    }


    if ( atoi(value) == 1 || !strcasecmp(value, "true") )
    {
        ret = adapter_set_property (property, TRUE);

    }

    /* "?value=" parameter is "0" or "false" */
    else if ( atoi(value) == 0 || !strcasecmp(value, "false") )
    {
        ret = adapter_set_property (property, FALSE);
    }
    else
    {
        afb_req_fail (request, "failed", "Invalid value");
        return;
    }

    if (0 == ret)
    {
        afb_req_success (request, NULL, NULL);
    }
    else
    {
        afb_req_fail (request, "failed", "Bluetooth set property failed");
    }

}

static void eventadd (struct afb_req request)
{
	const char *tag = afb_req_value(request, "tag");
	const char *name = afb_req_value(request, "name");

	if (tag == NULL || name == NULL)
		afb_req_fail(request, "failed", "bad arguments");
	else if (0 != event_add(tag, name))
		afb_req_fail(request, "failed", "creation error");
	else
		afb_req_success(request, NULL, NULL);
}

static void eventdel (struct afb_req request)
{
	const char *tag = afb_req_value(request, "tag");

	if (tag == NULL)
		afb_req_fail(request, "failed", "bad arguments");
	else if (0 != event_del(tag))
		afb_req_fail(request, "failed", "deletion error");
	else
		afb_req_success(request, NULL, NULL);
}

static void eventsub (struct afb_req request)
{
	const char *tag = afb_req_value(request, "tag");

	if (tag == NULL)
		afb_req_fail(request, "failed", "bad arguments");
	else if (0 != event_subscribe(request, tag))
		afb_req_fail(request, "failed", "subscription error");
	else
		afb_req_success(request, NULL, NULL);
}

static void eventunsub (struct afb_req request)
{
	const char *tag = afb_req_value(request, "tag");

	if (tag == NULL)
		afb_req_fail(request, "failed", "bad arguments");
	else if (0 != event_unsubscribe(request, tag))
		afb_req_fail(request, "failed", "unsubscription error");
	else
		afb_req_success(request, NULL, NULL);
}

static void eventpush (struct afb_req request)
{
	const char *tag = afb_req_value(request, "tag");
	const char *data = afb_req_value(request, "data");
	json_object *object = data ? json_tokener_parse(data) : NULL;

	if (tag == NULL)
		afb_req_fail(request, "failed", "bad arguments");
	else if (0 > event_push(object, tag))
		afb_req_fail(request, "failed", "push error");
	else
		afb_req_success(request, NULL, NULL);
}

/*
 * broadcast new device
 */
void bt_broadcast_device_added(struct btd_device *BDdevice)
{
    unsigned int filter = BD_ADDER|BD_NAME|BD_PAIRED|BD_ACLCONNECTED|BD_AVCONNECTED|BD_HFPCONNECTED;
    int ret;
    json_object *jresp = new_json_object_from_device(BDdevice, filter);

    LOGD("\n");
    ret = event_push(jresp,"device_added");
    //ret = afb_daemon_broadcast_event(afbitf->daemon, "device_added", jresp);
    LOGD("%d\n",ret);
}

/*
 * broadcast device removed
 */
void bt_broadcast_device_removed(struct btd_device *BDdevice)
{
    unsigned int filter = BD_ADDER;
    int ret;
    json_object *jresp = new_json_object_from_device(BDdevice, filter);

    LOGD("\n");
    ret = event_push(jresp,"device_removed");
    //ret = afb_daemon_broadcast_event(afbitf->daemon, "device_removed", jresp);
    LOGD("%d\n",ret);
}


/*
 * broadcast device updated
 */
void bt_broadcast_device_properties_change(struct btd_device *BDdevice)
{

    unsigned int filter = BD_ADDER|BD_NAME|BD_PAIRED|BD_ACLCONNECTED|BD_AVCONNECTED|BD_HFPCONNECTED;
    int ret;
    json_object *jresp = new_json_object_from_device(BDdevice, filter);

    LOGD("\n");
    ret = event_push(jresp,"device_updated");
    //ret = afb_daemon_broadcast_event(afbitf->daemon, "device_updated", jresp);
    LOGD("%d\n",ret);
}

/*
 * broadcast request confirmation
 */
gboolean bt_request_confirmation(const gchar *device, guint passkey)
{
    json_object *jresp = json_object_new_object();
    json_object *jstring = NULL;
    int ret;

    jstring = json_object_new_string(device);
    json_object_object_add(jresp, "Address", jstring);
    jstring = json_object_new_int(passkey);
    json_object_object_add(jresp, "Passkey", jstring);

    ret = event_push(jresp,"request_confirmation");
    //ret = afb_daemon_broadcast_event(afbitf->daemon, "device_updated", jresp);

    LOGD("%d\n",ret);

    if (ret >0) {
        return TRUE;
    }else {
        return FALSE;
    }

}

static void bt_send_confirmation(struct afb_req request)
{
    const char *value = afb_req_value(request, "value");
    int ret;
    gboolean confirmation;

    if (!value) {
            afb_req_fail (request, "failed", "Unable to get value status");
            return;
    }

    /* "?value=" parameter is "1" or "yes" */
    else if ( atoi(value) == 1 || !strcasecmp(value, "yes") )
    {
        ret = agent_send_confirmation (TRUE);
    }

    /* "?value=" parameter is "0" or "no" */
    else if ( atoi(value) == 0 || !strcasecmp(value, "no") )
    {
        ret = agent_send_confirmation (TRUE);
    }
    else
    {
        afb_req_fail (request, "failed", "Invalid value");
        return;
    }

    if ( 0==ret) {
        afb_req_success(request, NULL, NULL);
    }else {
        afb_req_success(request, "failed", "fail");
    }

}

/*
 * array of the verbs exported to afb-daemon
 */
static const struct afb_verb_desc_v1 binding_verbs[]= {
/* VERB'S NAME                    SESSION MANAGEMENT                FUNCTION TO CALL                    SHORT DESCRIPTION */
{ .name = "power",               .session = AFB_SESSION_NONE,      .callback = bt_power,               .info = "Set Bluetooth Power ON or OFF" },
{ .name = "start_discovery",     .session = AFB_SESSION_NONE,      .callback = bt_start_discovery,     .info = "Start discovery" },
{ .name = "stop_discovery",      .session = AFB_SESSION_NONE,      .callback = bt_stop_discovery,      .info = "Stop discovery" },
{ .name = "discovery_result",    .session = AFB_SESSION_NONE,      .callback = bt_discovery_result,    .info = "Get discovery result" },
{ .name = "remove_device",       .session = AFB_SESSION_NONE,      .callback = bt_remove_device,       .info = "Remove the special device" },
{ .name = "pair",                .session = AFB_SESSION_NONE,      .callback = bt_pair,                .info = "Pair to special device" },
{ .name = "cancel_pair",         .session = AFB_SESSION_NONE,      .callback = bt_cancel_pairing,      .info = "Cancel the pairing process" },
{ .name = "connect",             .session = AFB_SESSION_NONE,      .callback = bt_connect,             .info = "Connect to special device" },
{ .name = "disconnect",          .session = AFB_SESSION_NONE,      .callback = bt_disconnect,          .info = "Disconnect special device" },
{ .name = "set_device_property", .session = AFB_SESSION_NONE,      .callback = bt_set_device_property, .info = "Set special device property" },
{ .name = "set_property",        .session = AFB_SESSION_NONE,      .callback = bt_set_property,        .info = "Set Bluetooth property" },
{ .name = "send_confirmation",   .session = AFB_SESSION_NONE,      .callback = bt_send_confirmation,   .info = "Send Confirmation" },
{ .name = "eventadd",            .session = AFB_SESSION_NONE,      .callback = eventadd,               .info = "adds the event of 'name' for the 'tag'"},
{ .name = "eventdel",            .session = AFB_SESSION_NONE,      .callback = eventdel,               .info = "deletes the event of 'tag'"},
{ .name = "eventsub",            .session = AFB_SESSION_NONE,      .callback = eventsub,               .info = "subscribes to the event of 'tag'"},
{ .name = "eventunsub",          .session = AFB_SESSION_NONE,      .callback = eventunsub,             .info = "unsubscribes to the event of 'tag'"},
{ .name = "eventpush",           .session = AFB_SESSION_NONE,      .callback = eventpush,              .info = "pushs the event of 'tag' with the 'data'"},

{ .name = NULL } /* marker for end of the array */
};

/*
 * description of the binding for afb-daemon
 */
static const struct afb_binding binding_description =
{
    .type   = AFB_BINDING_VERSION_1,
    .v1 = {
        .info   = "Application Framework Binder - Bluetooth Manager plugin",
        .prefix = "Bluetooth-Manager",
        .verbs   = binding_verbs
    }
};

/*
 * activation function for registering the binding called by afb-daemon
 */
const struct afb_binding *afbBindingV1Register (const struct afb_binding_interface *itf)
{
    afbitf = itf;         // records the interface for accessing afb-daemon

#if 1
//temp solution to fix configure Bluetooth USB Dongle
    system("rfkill unblock bluetooth");
    system("hciconfig hci0 up");
#endif

    Binding_RegisterCallback_t API_Callback;
    API_Callback.binding_device_added = bt_broadcast_device_added;
    API_Callback.binding_device_removed = bt_broadcast_device_removed;
    API_Callback.binding_device_properties_changed = bt_broadcast_device_properties_change;
    API_Callback.binding_request_confirmation = bt_request_confirmation;
    BindingAPIRegister(&API_Callback);

    BluetoothManagerInit();

    return &binding_description;
}

#if 0
int afbBindingV1ServiceInit(struct afb_service service)
{
    return BluetoothManageInit();
}
#endif


/***************************** The End Of File ******************************/

