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
#include "bluetooth-api.h"
#include "bluetooth-manager.h"


/*
 * the interface to afb-daemon
 */
const struct afb_binding_interface *afbitf;


/* ------ PUBLIC PLUGIN FUNCTIONS --------- */

/**/
static void bt_power (struct afb_req request) 
{
    D_PRINTF("\n");

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
                afb_req_fail (request, "failed", "no more radio devices available");
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

        json_object_object_add (jresp, "power", json_object_new_string ("off"));
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
    D_PRINTF("\n");
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
    D_PRINTF("\n");
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
    D_PRINTF("\n");
    GSList *list = NULL;
    adapter_update_devices();
    list = adapter_get_devices_list();
    if (NULL == list)
    {
        afb_req_fail (request, "failed", "No find devices");
        return;
    }

    json_object *my_array = json_object_new_array();

    for(;list;list=list->next)
    {
        struct btd_device *BDdevice = list->data;
        //D_PRINTF("\n%s\t%s\n",BDdevice->bdaddr,BDdevice->name);

        json_object *jresp = json_object_new_object();
        json_object *jstring1 = NULL;
        json_object *jstring2 = NULL;  
        json_object *jstring3 = NULL;
        json_object *jstring4 = NULL;
        json_object *jstring5 = NULL;
        json_object *jstring6 = NULL;



        if (BDdevice->bdaddr)
        {
            jstring1 = json_object_new_string(BDdevice->bdaddr); 
        }
        else
        {
            jstring1 = json_object_new_string("");
        }

        
        if (BDdevice->name)
        {
            jstring2 = json_object_new_string(BDdevice->name);
        }
        else
        {
            jstring2 = json_object_new_string("");
        }

        jstring3 = (TRUE == BDdevice->paired) ? json_object_new_string("True"):json_object_new_string("False");
        jstring4 = (TRUE == BDdevice->connected) ? json_object_new_string("True"):json_object_new_string("False");
        jstring5 = (TRUE == isAVPConnected(BDdevice)) ? json_object_new_string("True"):json_object_new_string("False");
        jstring6 = (TRUE == isHFPConnected(BDdevice)) ? json_object_new_string("True"):json_object_new_string("False");

        
        json_object_object_add(jresp, "Address", jstring1);
        json_object_object_add(jresp, "Name", jstring2);
        json_object_object_add(jresp, "Paired", jstring3);
        json_object_object_add(jresp, "Connected", jstring4);
        json_object_object_add(jresp, "AVPConnected", jstring5);
        json_object_object_add(jresp, "HFPConnected", jstring6);
        json_object_array_add(my_array, jresp);
    }

    afb_req_success(request, my_array, "BT - Scan Result is Displayed");
}

/**/
static void bt_pair (struct afb_req request) 
{
    D_PRINTF("\n");

    const char *value = afb_req_value (request, "value");
    int ret = 0;
    GSList *list = NULL;

    if (NULL == value)
    {
        afb_req_fail (request, "failed", "Please Input the Device Address");
        return;
    }
    
    list = adapter_get_devices_list();
    
    for(;list;list=list->next)
    {
        struct btd_device *BDdevice = list->data;
        
        if ((NULL!=BDdevice->bdaddr)&&g_strrstr(BDdevice->bdaddr,value))
        {
            D_PRINTF("\n%s\t%s\n",BDdevice->bdaddr,BDdevice->name);
            ret = device_pair(BDdevice);
            if (0 == ret)
            {
                afb_req_success (request, NULL, NULL);
            }
            else
            {
                afb_req_fail (request, "failed", "Device pairing failed"); 
            }
            return;
        }
    }
    
    afb_req_fail (request, "failed", "Not found device");   

}

/**/
static void bt_cancel_pairing (struct afb_req request) 
{
    D_PRINTF("\n");

    const char *value = afb_req_value (request, "value");
    int ret = 0;
    GSList *list = NULL;

    if (NULL == value)
    {
        afb_req_fail (request, "failed", "Please Input the Device Address");
        return;
    }
    
    list = adapter_get_devices_list();
    
    for(;list;list=list->next)
    {
        struct btd_device *BDdevice = list->data;
        
        if ((NULL!=BDdevice->bdaddr)&&g_strrstr(BDdevice->bdaddr,value))
        {
            D_PRINTF("\n%s\t%s\n",BDdevice->bdaddr,BDdevice->name);
            ret = device_cancelPairing(BDdevice);
            if (0 == ret)
            {
                afb_req_success (request, NULL, NULL);
            }
            else
            {
                afb_req_fail (request, "failed", "Device pairing failed"); 
            }
            return;
        }
    }
    
    afb_req_fail (request, "failed", "Not found device");   

}

/**/
static void bt_connect (struct afb_req request) 
{
    D_PRINTF("\n");

    const char *value = afb_req_value (request, "value");
    int ret = 0;
    GSList *list = NULL;

    if (NULL == value)
    {
        afb_req_fail (request, "failed", "Please Input the Device Address");
        return;
    }
    
    list = adapter_get_devices_list();
    
    for(;list;list=list->next)
    {
        struct btd_device *BDdevice = list->data;
        
        if ((NULL!=BDdevice->bdaddr)&&g_strrstr(BDdevice->bdaddr,value))
        {
            D_PRINTF("\n%s\t%s\n",BDdevice->bdaddr,BDdevice->name);
            ret = device_connect(BDdevice);
            if (0 == ret)
            {
                afb_req_success (request, NULL, NULL);
            }
            else
            {
                afb_req_fail (request, "failed", "Device pairing failed"); 
            }
            return;
        }
    }
    
    afb_req_fail (request, "failed", "Not found device");  
}

/**/
static void bt_disconnect (struct afb_req request) 
{
    D_PRINTF("\n");

    const char *value = afb_req_value (request, "value");
    int ret = 0;
    GSList *list = NULL;

    if (NULL == value)
    {
        afb_req_fail (request, "failed", "Please Input the Device Address");
        return;
    }
    
    list = adapter_get_devices_list();
    
    for(;list;list=list->next)
    {
        struct btd_device *BDdevice = list->data;
        
        if ((NULL!=BDdevice->bdaddr)&&g_strrstr(BDdevice->bdaddr,value))
        {
            D_PRINTF("\n%s\t%s\n",BDdevice->bdaddr,BDdevice->name);
            ret = device_disconnect(BDdevice);
            if (0 == ret)
            {
                afb_req_success (request, NULL, NULL);
            }
            else
            {
                afb_req_fail (request, "failed", "Device pairing failed"); 
            }
            return;
        }
    }
    
    afb_req_fail (request, "failed", "Not found device");  
}

/**/
static void bt_remove_device (struct afb_req request) 
{
    D_PRINTF("\n");

    const char *value = afb_req_value (request, "value");
    int ret = 0;
    GSList *list = NULL;

    if (NULL == value)
    {
        afb_req_fail (request, "failed", "Please Input the Device Address");
        return;
    }
    
    list = adapter_get_devices_list();
    
    for(;list;list=list->next)
    {
        struct btd_device *BDdevice = list->data;
        
        if ((NULL!=BDdevice->bdaddr)&&g_strrstr(BDdevice->bdaddr,value))
        {
            D_PRINTF("\n%s\t%s\n",BDdevice->bdaddr,BDdevice->name);
            ret = adapter_remove_device(BDdevice);
            if (0 == ret)
            {
                afb_req_success (request, NULL, NULL);
            }
            else
            {
                afb_req_fail (request, "failed", "Device pairing failed"); 
            }
            return;
        }
    }
    
    afb_req_fail (request, "failed", "Not found device");   
}


/**/
static void bt_set_property (struct afb_req request) 
{
    D_PRINTF("\n");

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
    
    list = adapter_get_devices_list();
    
    for(;list;list=list->next)
    {
        struct btd_device *BDdevice = list->data;
        
        if ((NULL!=BDdevice->bdaddr)&&g_strrstr(BDdevice->bdaddr,address))
        {
            //D_PRINTF("\n%s\t%s\n",BDdevice->bdaddr,BDdevice->name);
            ret = device_set_property(BDdevice, property, value);
            if (0 == ret)
            {
                afb_req_success (request, NULL, NULL);
            }
            else
            {
                afb_req_fail (request, "failed", "Device set property failed"); 
            }
            return;
        }
    }
    
    afb_req_fail (request, "failed", "Not found device");   
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
{ .name = "set_property",          .session = AFB_SESSION_NONE,      .callback = bt_set_property,          .info = "Set special device property" },

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
    //D_PRINTF("\n");
#if 1
//temp solution to fix configure Bluetooth USB Dongle
    system("rfkill unblock bluetooth");
    system("hciconfig hci0 up");
#endif
    BluetoothManageInit();
    return &binding_description;
}

#if 0
int afbBindingV1ServiceInit(struct afb_service service)
{
    return BluetoothManageInit();
}
#endif


/************************************** The End Of File **************************************/  

