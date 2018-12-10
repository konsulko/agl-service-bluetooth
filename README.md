# Bluetooth Service

## Overview

Bluetooth service uses the respective BlueZ package to connect to bluetooth devices

## Verbs

| Name               | Description                                             | JSON Response                                                           |
|--------------------|---------------------------------------------------------|-------------------------------------------------------------------------|
| subscribe          | subscribe to bluetooth events                           | *Request:* {"value": "device_changes"}                                  |
| unsubscribe        | unsubscribe to bluetooth events                         | *Request:* {"value": "device_changes"}                                  |
| managed_objects    | retrieve managed bluetooth devices                      | see managed_objects verb section                                        |
| adapter_state      | retrieve or change adapter scan settings                | see adapter_state verb section                                          |
| default_adapter    | retrieve or change default adapter setting              | *Request:* {"adapter": "hci1"}                                          |
| avrcp_controls     | avrcp controls for MediaPlayer1 playback                | see avrcp_controls verb section                                         |
| connect            | connect to already paired device                        | see connect/disconnect verb section                                     |
| disconnect         | disconnect to already connected device                  | see connect/disconnect verb section                                     |
| pair               | initialize a pairing request                            | *Request:* {"device":"dev_88_0F_10_96_D3_20"}                           |
| cancel_pairing     | cancel an outgoing pair request                         |                                                                         |
| confirm_pairing    | confirm incoming/outgoing bluetooth pairing pincode     | *Request:* {"pincode": 31415}                                           |
| remove_device      | remove already paired device                            | *Request:* {"device": "dev_88_0F_10_96_D3_20"}                          |


### managed_objects verb

This verb allows an client to get initial paired devices, and discovered unpaired devices before subscriptio to *devices_changed* event.

<pre>
{
  "response": {
    "adapters": [
      {
        "name": "hci0",
        "properties": {
          "address": "00:1A:7D:DA:71:0F",
          "powered": true,
          "discoverable": true,
          "discoverabletimeout": 180,
          "pairable": true,
          "pairabletimeout": 0,
          "discovering": true,
          "uuids": [
            "00001112-0000-1000-8000-00805f9b34fb",
            "00001801-0000-1000-8000-00805f9b34fb",
            "0000110e-0000-1000-8000-00805f9b34fb",
            "00001800-0000-1000-8000-00805f9b34fb",
            "00001200-0000-1000-8000-00805f9b34fb",
            "0000110c-0000-1000-8000-00805f9b34fb",
            "0000110b-0000-1000-8000-00805f9b34fb",
            "0000110a-0000-1000-8000-00805f9b34fb",
            "0000111e-0000-1000-8000-00805f9b34fb",
            "0000111f-0000-1000-8000-00805f9b34fb",
            "00001108-0000-1000-8000-00805f9b34fb"
          ]
        }
      }
    ],
    "devices": [
      {
        "adapter": "hci0",
        "device": "dev_F8_34_41_DA_BA_46",
        "properties": {
          "address": "F8:34:41:DA:BA:46",
          "name": "roguebox",
          "alias": "roguebox",
          "class": 1835276,
          "icon": "computer",
          "paired": false,
          "trusted": false,
          "blocked": false,
          "legacypairing": false,
          "rssi": -63,
          "connected": false,
          "uuids": [
            "0000110e-0000-1000-8000-00805f9b34fb",
            "0000110c-0000-1000-8000-00805f9b34fb",
            "00001112-0000-1000-8000-00805f9b34fb",
            "00001108-0000-1000-8000-00805f9b34fb",
            "00001133-0000-1000-8000-00805f9b34fb",
            "00001132-0000-1000-8000-00805f9b34fb",
            "0000112f-0000-1000-8000-00805f9b34fb",
            "00001104-0000-1000-8000-00805f9b34fb",
            "00001106-0000-1000-8000-00805f9b34fb",
            "00001105-0000-1000-8000-00805f9b34fb",
            "0000110a-0000-1000-8000-00805f9b34fb",
            "0000110b-0000-1000-8000-00805f9b34fb",
            "00005005-0000-1000-8000-0002ee000001"
          ],
          "modalias": "usb:v1D6Bp0246d0530",
          "adapter": "/org/bluez/hci0",
          "txpower": 12,
          "servicesresolved": false
        }
      },
      {
        "adapter": "hci0",
        "device": "dev_67_13_E2_57_29_0F",
        "properties": {
          "address": "68:13:E2:57:29:0F",
          "alias": "67-13-E2-57-29-0F",
          "paired": false,
          "trusted": false,
          "blocked": false,
          "legacypairing": false,
          "rssi": -69,
          "connected": false,
          "uuids": [],
          "adapter": "/org/bluez/hci0",
          "servicesresolved": false
        }
      },
    ],
    "transports": [
      {
          "endpoint": "fd1",
          "adapter": "hci0",
          "device": "dev_D0_81_7A_5A_BC_5E",
          "properties": {
            "uuid": "0000110B-0000-1000-8000-00805F9B34FB",
            "state": "idle",
            "volume": 127
          }
      }
   ],
}
</pre>


### adapter_state verb

adapter_state verb allows setting and retrieving of requested adapter settings:

| Name            | Description                                                            |
|-----------------|------------------------------------------------------------------------|
| adapter         | Must be the name of the adapter (i.e. hci0)                            |
| discovery       | Discover nearby broadcasting devices                                   |
| discoverable    | Allow other devices to detect this device                              |
| powered         | Adapter power state (optional, rfkill should be disabled already)      |
| filter          | Display devices only with respective UUIDS listed (write only setting) |

#### avrcp_controls verb

avrcp_controls verb allow controlling the playback of the defined device

| Name            | Description                                                                                  |
|-----------------|----------------------------------------------------------------------------------------------|
| adapter         | Name of the adapter (i.e. hci0)                                                              |
| device          | Must be the name of the device (i.e. dev_88_0F_10_96_D3_20)                                  |
| action          | Playback control action to take (e.g Play, Pause, Stop, Next, Previous, FastForward, Rewind) |

### connect/disconnect verbs

NOTE: uuid in this respect is not related to the afb framework but the Bluetooth profile UUID

To connect/disconnect using the respective verb with all known and authenticated profiles:

<pre>
  {"device": "dev_88_0F_10_96_D3_20"}
</pre>

To do the same for the respective device, verb, and for singular profile

<pre>
  {"device": "dev_88_0F_10_96_D3_20", "uuid": "0000110e-0000-1000-8000-00805f9b34fb"}
</pre>

## Events

| Name              | Description                              | JSON Event Data                           |
|-------------------|------------------------------------------|-------------------------------------------|
| device_changes    | report on bluetooth devices              | see device_changes event section          |
| media             | report on MediaPlayer1 events            | see media event section                   |
| agent             | PIN from BlueZ agent for confirmation    | see agent event section                   |


### device_changes event

Sample of discovering a new device event:

<pre>
{
  "adapter": "hci0",
  "device": "dev_88_0F_10_96_D3_20",
  "action": "added",
  "properties": {
    "address": "88:0F:10:96:D3:20",
    "name": "MI_SCALE",
    "alias": "MI_SCALE",
    "class": 7995916,
    "icon": "phone",
    "paired": false,
    "trusted": false,
    "blocked": false,
    "legacypairing": false,
    "rssi": -55,
    "connected": false,
    "uuids": [
      "00001200-0000-1000-8000-00805f9b34fb",
      "0000111f-0000-1000-8000-00805f9b34fb",
      "0000112f-0000-1000-8000-00805f9b34fb",
      "0000110a-0000-1000-8000-00805f9b34fb",
      "0000110c-0000-1000-8000-00805f9b34fb",
      "00001116-0000-1000-8000-00805f9b34fb",
      "00001132-0000-1000-8000-00805f9b34fb",
      "00000000-deca-fade-deca-deafdecacafe",
      "02030302-1d19-415f-86f2-22a2106a0a77",
      "2d8d2466-e14d-451c-88bc-7301abea291a"
    ],
    "adapter": "/org/bluez/hci0",
    "servicesresolved": false
  }
}
</pre>

Changed status events for a device:

<pre>
{
  "adapter": "hci0",
  "device": "dev_88_0F_10_96_D3_20",
  "action": "changed",
  "properties": {
    "connected": true
  }
}
</pre>

### media event

Playing audio reporting event (not all fields will be passed in every event):

<pre>
{
        "adapter": "hci0",
        "device": "dev_D0_81_7A_5A_BC_5E",
        "type": "playback",
        "track": {
                "title": "True Colors",
                "duration": 228000,
                "album": "True Colors",
                "tracknumber": 6,
                "artist": "Zedd",
                "numberoftracks": 11,
                "genre": "Dance & DJ/General"
        },
        "position": 5600,
        "status": "playing"
        "connected": false
}
</pre>

A2DP transport addition/removal (some fields are optional):

<pre>
{
        "adapter": "hci0",
        "device": "dev_D0_81_7A_5A_BC_5E",
        "action": "added",
        "type": "transport",
        "endpoint": "fd0"
        "properties": {
                "uuid": "0000110B-0000-1000-8000-00805F9B34FB",
                "state": "idle",
                "volume": 127
         },
}
...
{
        "adapter": "hci0",
        "device": "dev_D0_81_7A_5A_BC_5E",
        "action": "removed",
        "type": "transport",
        "endpoint": "fd0"
}
</pre>

### agent event

After pairing request agent will send event for a pincode that must be confirmed on both sides:

<pre>
{
  "adapter": "hci0",
  "device": "dev_88_OF_10_96_D3_20",
  "action": "request_confirmation",
  "pincode": 327142
}
</pre>

If pairing is canceled or fails:

<pre>
{
  "action": "canceled_pairing"
}
</pre>
