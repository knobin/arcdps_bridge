# Connecting to the extension

The extension uses the Win32 API for [Named Pipes](https://docs.microsoft.com/en-us/windows/win32/ipc/named-pipes), and the client must therefore be able to support them as well. Multiple clients are supported as well. Setting up the client / server connection will always use events in JSON format. After succesfull connection to the server it will send events in the selected protocol.

## Table of Contents

- [Creating a Named Pipe](#creating-a-named-pipe)
  - [Disconnecting](#disconnecting)
- [Connection Status event](#connection-status-event)
  - [Example of Connection Status event](#example-of-connection-status-event)
- [Bridge Information event](#bridge-information-event)
  - [Example of BridgeInfo event](#example-of-bridgeinfo-event)
  - [BridgeEvent in serial form](#bridgeevent-in-serial-form)
- [Subscribe to events](#subscribe-to-events)
  - [Subscribe selection](#subscribe-selection)
  - [Protocol selection](#protocol-selection)
  - [Example of subscribe event](#example-of-subscribe-event)
- [Status event](#status-event)
  - [Example of status event](#example-of-status-event)
  - [Status event error messages](#status-event-error-messages)
  - [After receiving status event](#after-receiving-status-event)
- [Closing event](#closing-event)
  - [Example of closing event](#example-of-closing-event)
- [After connection and subscription have been established](#after-connection-and-subscription-have-been-established)

## Creating a Named Pipe

This is different depending on the programming language that is used. To create a pipe to the extension, use the follow pipe name: 

```
arcdps-bridge
``` 

The important thing for the pipe is that it must be able to both read, and write. Because the client must be able to respond on a certain message to be able to receive events.

For example on how to open a Named Pipe see the [examples](../examples).

### Disconnecting

If you have an established connection to the extension and wish to disconnect. Just close the named pipe, there is no need to tell the extension you want to disconnect or terminate the connection. It will detect that you have done that.

## Connection Status event

After the client has successfully established a Named Pipe, the extension will requires some communication between the client, and the server before starting to send events. The server will initially send a Connection Status event to the client describing information about itself. During the setup communication process all events will be in JSON form. The Connection Status will only be sent here and never in any other form (for example in serial).

The event includes (shown in JSON form):
- ```category```: Category of event, always ```Info``` for BridgeEvent.
- ```type```: Type of event, always ```BridgeInfo``` for BridgeEvent.
- ```id```: Identifier of the event.
- ```timestamp```: Elapsed time since epoch.
- ```data.majorApiVersion```: Major API version of the extension.
- ```data.minorApiVersion```: Minor API version of the extension.
- ```data.version```: Version of the extension in string form.
- ```data.info```: Contains the data object from the Bridge Information event (see [Bridge Information event](#bridge-information-event) below for details).
- ```data.types```: List of all available MessageType that can be used for subscribing.
- ```data.success```: If server has acknowledged the connection and successfully allocated resources (if false connection is terminated after this event).
- ```data.error```: If success is false an error message will be present here (if success is true, this won't exists).

### Example of Connection Status event

```json
{
    "category": "Info",
    "type": "ConnectionStatus",
    "id": 410091,
    "timestamp": 1678614165885,
    "data":
    {
        "info":
        {
            "arcLoaded": true,
            "arcVersion": "20230301.180021-480-x64",
            "extrasFound": true,
            "extrasInfoVersion": 2,
            "extrasLoaded": true,
            "extrasVersion": "1.10.2.1",
            "validator": 2
        },
        "majorApiVersion": 2,
        "minorApiVersion": 1,
        "success": true,
        "types": ["ConnectionStatus", "BridgeInfo", "Status", "Closing", "CombatEvent", "ExtrasSquadUpdate", "ExtrasLanguageChanged", "ExtrasKeyBindChanged", "ExtrasChatMessage", "SquadStatus", "SquadAdd", "SquadUpdate", "SquadRemove"],
        "version": "2.0.0"
    }
}
```

## Bridge Information event

The Bridge Information event contains runtime information about the bridge. It will included in the Connection Status event when connecting to the server, but if any data fields gets updated during execution this event will be sent to all connected clients (in the selected protocol). 

The event includes (shown in JSON form):
- ```category```: Category of event, always ```Info``` for BridgeEvent.
- ```type```: Type of event, always ```BridgeInfo``` for BridgeEvent.
- ```id```: identifier of the event.
- ```timestamp```: elapsed time since epoch.
- ```data.arcLoaded```: Is ArcDPS used in the bridge, either true or false.
- ```data.arcVersion```: Version of ArcDPS in string form.
- ```data.extrasFound```: Is ArcDPS Unofficial Extras present, either true or false.
- ```data.extrasInfoVersion```: Version of ArcDPS Unofficial Extras API.
- ```data.extrasLoaded```: Is ArcDPS Unofficial Extras used in the bridge, either true or false.
- ```data.extrasVersion```: Version of ArcDPS Unofficial Extras in string form (empty string if extrasLoaded is false).
- ```data.validator```: Indicates what version of bridge information this is. If any value changes this will be incremented. Initial value is ```1```.

### Example of BridgeInfo event

```json
{
    "category": "Info",
    "type": "BridgeInfo",
    "id": 1730791,
    "timestamp": 1678615043581,
    "data":
    {
      "arcLoaded": true,
      "arcVersion": "20230301.180021-480-x64",
      "extrasFound": true,
      "extrasInfoVersion": 2,
      "extrasLoaded": true,
      "extrasVersion": "1.10.2.1",
      "validator": 2
    }
}
```

Due to how ArcDPS and Unofficial Extras is loaded, the latter will not be available right away until about 2 seconds after the bridge extension has been loaded. This means that if a client connects in this 2 seconds window, Unofficial Extras will not be available in the bridge information. The server will correct this by sending a new version of the bridge information event to connected clients when (and if) Unofficial Extras has been found. This event will be sent in the selected protocol!

### BridgeEvent in serial form

Bytes are represented in a big endian for simplicity. It is assumed that the client is running on the same machine and therefore have the same endianness.

For an explanation on how the serialization works in this extension have a look at [Events.md](Events.md).

```
0x01 0x01 0x00 0x00 0x00 0x00 0x00 0x1A 0x68 0xE7 0x00 0x00 0x01 0x83 0xD5 0xA9 0x57 0xE4 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x02 0x31 0x2E 0x36 0x2E 0x31 0x2E 0x31 0x00 0x32 0x30 0x32 0x32 0x30 0x39 0x31 0x34 0x2E 0x31 0x38 0x30 0x33 0x32 0x33 0x2D 0x34 0x35 0x34 0x2d 0x78 0x36 0x34 0x00 0x00 0x00 0x00 0x02 0x01 0x01 0x01
```

Interpreted as:

```
0x01 // category is one byte
0x01 // type is one byte
0x00 0x00 0x00 0x00 0x00 0x1A 0x68 0xE7 // id is eight bytes (uint64_t) 
0x00 0x00 0x01 0x83 0xD5 0xA9 0x57 0xE4 // timestamp is eight bytes (uint64_t)

0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x02 // validator is eight bytes (uint64_t)

0x31 0x2E 0x36 0x2E 0x31 0x2E 0x31 0x00 // extrasVersion "1.6.1.1" and null terminator
0x32 0x30 0x32 0x32 0x30 0x39 0x31 0x34 0x2E 0x31 0x38 0x30 0x33 0x32 0x33 0x2D 0x34 0x35 0x34 0x2d 0x78 0x36 0x34 0x00 // arcVersion "20220914.180323-454-x64" and null terminator

0x00 0x00 0x00 0x02 // extrasInfoVersion is four bytes (uint32_t)

0x01 // arcLoaded is one byte
0x01 // extrasFound is one byte
0x01 // extrasLoaded is one byte
```

## Subscribe to events

The client is expected to send a JSON object with a ```subscribe``` entry and a ```protocol``` entry. 

### Subscribe selection

This value is determined by what event types you want to receive, and are based on the ```MessageType``` enum class internally in [Message.hpp](../src/Message.hpp).

```cpp
enum MessageType
{
    // Info types.
    ConnectionStatus,
    BridgeInfo,
    Status,
    Closing,

    // ArcDPS combat api types.
    CombatEvent,

    // Extras event types.
    ExtrasSquadUpdate,
    ExtrasLanguageChanged,
    ExtrasKeyBindChanged,
    ExtrasChatMessage,

    // Squad event types.
    SquadStatus,
    SquadAdd,
    SquadUpdate,
    SquadRemove
};
```

You can subscribe to any or all of these events by including the types in string form in a JSON array. It must be an array, if you only want 1 type is should still be included in an array.

**Values are case-sensitive!**

```cpp
subscribe = ["CombatEvent", "ExtrasChatMessage", "SquadStatus"]
```

Info related message types ("ConnectionStatus", "BridgeInfo", "Status", "Closing") are required and will be sent to the client. You should include these in your subscribtion for sake of compatibility and readability.

### Protocol selection

This value is determined by what format you want the events, and are based on the ```MessageProtocol``` enum class internally in [Message.hpp](../src/Message.hpp).

```cpp
enum MessageProtocol
{
    Serial,
    JSON
};
```

You can only use one protocol at a time and the selection must be in a string format.

**Values are case-sensitive!**

```cpp
protocol = "JSON";
```

### Example of subscribe event

Example of a subscribe event sent to the extension that subscribes to only squad events: 

```json
{
    "protocol": "JSON",
    "subscribe": ["ConnectionStatus", "BridgeInfo", "Status", "Closing", "SquadStatus", "SquadAdd", "SquadUpdate", "SquadRemove"]
}
```

## Status event

After the subscribe event has been sent to the extension, it will be evaluated, and a status event will be sent back indicating if it will be starting to send events. The Status event will only be sent in the setup process, so it can only be sent in JSON form.

The event includes (shown in JSON form):
- ```category```: Category of event, always ```Info``` for status event.
- ```type```: Type of event, always ```Status``` for status event.
- ```id```: identifier of the event.
- ```timestamp```: elapsed time since epoch.
- ```status.success```: either true or false.
- ```status.error```: Error message (only present if success is false).

### Example of status event 

```json
{
    "category": "Info",
    "type": "Status",
    "id": 1730792,
    "timestamp": 1665736988645,
    "data":
    {
        "success": false,
        "error": "No types are subscribed to"
    }
}
```

### Status event error messages

- ```Invalid JSON```: Subscribe message contains invalid JSON. 
- ```No types are subscribed to```: Could not subscribe to any message types with the provided ```subscribe``` value in the Subscribe event.
- ```No such protocol```: The ``protocol``` value in the Subscribe event does not match any supported protocol.
- ```Invalid Message Type "TYPE_HERE"```: The Message Type TYPE_HERE does not exist.


### After receiving status event

**All Status Messages where success is false are considered fatal and the connection will be closed.**

If success is false the pipe will be closed by the extension. If success is true, you now have a successful connection to the extension and will start to receive event in the selected protocol.

**Every event after this status event will be in the selected format!**

## Closing event

When the server is shutting down it will send a closing event to all connected clients to notify that no further events will be sent from the server. The event contains no data, only the message headers.

The event includes (shown in JSON form):
- ```category```: Category of event, always ```Info``` for BridgeEvent.
- ```type```: Type of event, always ```Closing``` for BridgeEvent.
- ```id```: identifier of the event.
- ```timestamp```: elapsed time since epoch.

### Example of closing event

Same event in JSON and Serial versions.

#### JSON

```json
{
    "category": "Info",
    "type": "Closing",
    "id": 2919227,
    "timestamp": 1665740375865
}
```

#### Serial

Bytes are represented in a big endian for simplicity. It is assumed that the client is running on the same machine and therefore have the same endianness.

```
0x01 0x03 0x00 0x00 0x00 0x00 0x00 0x2C 0x8B 0x3B 0x00 0x00 0x01 0x83 0xD5 0xDD 0x07 0x39
```

Interpreted as: 

```
0x01 // category is one byte
0x03 // type is one byte
0x00 0x00 0x00 0x00 0x00 0x2C 0x8B 0x3B // id is eight bytes (uint64_t) 
0x00 0x00 0x01 0x83 0xD5 0xDD 0x07 0x39 // timestamp is eight bytes (uint64_t)
```

## After connection and subscription have been established

After receiving a successful status event, the extension will now start sending the events requested by the client. The client can no longer send messages to the extension through the named pipe, and should only read from it.

Next step is to have a look at [Events.md](Events.md).

