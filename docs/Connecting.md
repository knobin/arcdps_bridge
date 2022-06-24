# Connecting to the extension

The extension uses the Win32 API for [Named Pipes](https://docs.microsoft.com/en-us/windows/win32/ipc/named-pipes), and the client must therefore be able to support them as well. Multiple clients are supported as well.

## Table of Contents

- [Creating a Named Pipe](#creating-a-named-pipe)
- [Bridge Information event](#bridge-information-event)
- [Subscribe to events](#subscribe-to-events)
- [Status event](#status-event)
- [After connection and subscribtion are established](#after-connection-and-subscribtion-are-established)

## Creating a Named Pipe

This is different depending on the programming language that is used. To create a pipe to the extension, use the follow pipe name: 

```
arcdps-bridge
``` 

The important thing for the pipe is that it must be able to both read, and write. Because the client must be able to respond on a certain message to be able to receive events.

For example on how to open a Named Pipe see the [examples](../examples/).

## Bridge Information event

After the client has successfully established a Named Pipe, the extension will requires some communication between the client, and the server before starting to send events. The server will initially send a BridgeInfo event to the client describing information about itself.

The event includes:
- ```type```: Type of event, always ```info``` for BridgeEvent.
- ```info.version```: Version of the extension in string form.
- ```info.arcLoaded```: Is ArcDPS loaded, either true or false.
- ```info.extrasLoaded```: Is ArcDPS Unofficial Extras loaded, either true or false.
- ```info.arcVersion```: Version of ArcDPS Unofficial Extras in string form (empty string if ArcLoaded is false).
- ```info.extrasVersion```: Version of ArcDPS in string form (empty string if ExtrasLoaded is false).

Example of BridgeInfo event: 

```json
{
    "type": "info",
    "info":
    {
        "version": "1.0.1",
        "extrasVersion": "1.3.1.1",
        "arcVersion": "20220525.171543-432-x64",
        "arcLoaded": true,
        "extrasLoaded": true
    }
}
```

Depending on the contents of this event, the client can decide on if it wants to subscribe to, or drop the connection altogether.

## Subscribe to events

The client is expected to send a JSON object with a single ```subscribe``` set to a number value. This value is determined by what event types you want to receive, and are based on the ```MessageType ``` enum class internally in [ApplicationData.hpp](../src/ApplicationData.hpp).

```cpp
enum class MessageType : uint8_t
{
    NONE = 0,
    Combat = 1,
    Extras = 2,
    Squad = 4
};
```

You can subscribe to any or all of these events using the Bitwise OR operator.

```cpp
subscribe = 1; // Only Combat events.
subscribe = 1 | 4; // Combat and Squad events.
```

Example of a subscribe event sent to the extension that subscribes to only squad events: 

```json
{
    "subscribe": 4
}
```

## Status event

After the subscrive event has been sent to the extension, it will be evaluated, and a status event will be sent back indicating if it will be starting to send events.

The event includes:
- ```type```: Type of event, always ```status``` for the status event.
- ```status.success```: either true or false.
- ```status.error```: Error message (only present if success is false).

Example of status event: 

```json
{
    "type": "status",
    "status":
    {
        "success": false,
        "error": "no subscription"
    }
}
```

If success is false the pipe will be closed by the extension.

## After connection and subscribtion are established

After receiving a successful status event, the extension will now start sending the events requested by the client. The client can no longer send messages to the extension through the named pipe, and should only read from it.

Next step is to have a look at [Events](Events.md).

