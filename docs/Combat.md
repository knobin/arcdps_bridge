# Combat event

Events directly from [ArcDPS combat API](https://www.deltaconnected.com/arcdps/api/).

## Table of Contents

- [ArcDPS combat callback](#arcdps-combat-callback)
- [Combat event details](#combat-event-details)
- [Other events](#other-events)

## ArcDPS combat callback

ArcDPS combat events are received from a combat callback (located in [Entry.cpp](../src/Entry.cpp)): 

```cpp
uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision)
```

Where ```cbtevent``` is the combat event:

```cpp
struct cbtevent
{
    uint64_t time;
    uint64_t src_agent;
    uint64_t dst_agent;
    int32_t value;
    int32_t buff_dmg;
    uint32_t overstack_value;
    uint32_t skillid;
    uint16_t src_instid;
    uint16_t dst_instid;
    uint16_t src_master_instid;
    uint16_t dst_master_instid;
    uint8_t iff;
    uint8_t buff;
    uint8_t result;
    uint8_t is_activation;
    uint8_t is_buffremove;
    uint8_t is_ninety;
    uint8_t is_fifty;
    uint8_t is_moving;
    uint8_t is_statechange;
    uint8_t is_flanking;
    uint8_t is_shields;
    uint8_t is_offcycle;
    uint8_t pad61;
    uint8_t pad62;
    uint8_t pad63;
    uint8_t pad64;
};
```

and, ```ag``` is agent short:

```cpp
struct ag
{
    char* name;
    uintptr_t id;
    uint32_t prof;
    uint32_t elite;
    uint32_t self;
    uint16_t team;
};
```

## Combat event details

All of the parameters from the ArcDPS combat callback is included in the Combat event, and every time the function is called (and a client has subscribed to the combat event) it will generate a JSON object from the data.

The event includes:
- ```type```: Type of event, always ```combat``` for Combat event.
- ```combat.id```: Callback parameter ```id```.
- ```combat.revision```: Callback parameter ```revision```.
- ```combat.ev```: Callback parameter ```ev``` (null if no ev).
- ```combat.src```: Callback parameter ```src``` (null if no src).
- ```combat.dst```: Callback parameter ```dst``` (null if no dst).
- ```combat.skillname```: Callback parameter ```skillname``` (null if no skillname).

Example of Combat event: 

```json
{
    "type": "combat",
    "combat":
    {
        "id": 45,
        "revision": 1,
        "ev":
        {
            "time": 128402538,
            "src_agent": 2000,
            "dst_agent": 0,
            "value": 760,
            "buff_dmg": 960,
            "overstack_value": 0,
            "skillid": 40729,
            "src_instid": 239,
            "dst_instid": 0,
            "src_master_instid": 0,
            "dst_master_instid": 0,
            "iff": 2,
            "buff": 0,
            "result": 0,
            "is_activation": 1,
            "is_buffremove": 0,
            "is_ninety": 1,
            "is_fifty": 0,
            "is_moving": 0,
            "is_statechange": 0,
            "is_flanking": 0,
            "is_shields": 0,
            "is_offcycle": 0
        },
        "src":
        {
            "name": "Knobin",
            "id": 2000,
            "prof": 4,
            "elite": 55,
            "self": 1,
            "team": 1863
        },
        "dst":
        {
            "name": "0",
            "id": 0,
            "prof": 0,
            "elite": 4294967295,
            "self": 0,
            "team": 0
        },
        "skillname": "Worldly Impact"
    }
}
```

## Other events

- [Extras](Extras.md) - ArcDPS Unofficial Extras events.
- [Squad](Squad.md) - Custom squad event.