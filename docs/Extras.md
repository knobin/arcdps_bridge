# Extras event

Events directly from [ArcDPS Unofficial Extras](https://github.com/Krappa322/arcdps_unofficial_extras_releases).

## Table of Contents

- [ArcDPS Unofficial Extras callback](#arcdps-unofficial-extras-callback)
- [Extras event details](#extras-event-details)
- [Other events](#other-events)

## ArcDPS Unofficial Extras callback

ArcDPS Unofficial Extras events are received from a callback (located in [Entry.cpp](../src/Entry.cpp)): 

```cpp
void squad_update_callback(const UserInfo* updatedUsers, uint64_t updatedUsersCount)
```

Where ```UserInfo``` is:

```cpp
struct UserInfo
{
    const char* AccountName;
    __time64_t JoinTime;
    UserRole Role;
    uint8_t Subgroup;
    bool ReadyStatus;
};
```

and, ```UserRole``` is: 

```cpp
enum class UserRole : uint8_t
{
	SquadLeader = 0,
	Lieutenant = 1,
	Member = 2,
	Invited = 3,
	Applied = 4,
	None = 5
};
```

## Extras event details

All the UserInfo array paramater in the ArcDPS Unofficial Extras callback will be sent one by one to the client.

The event includes:
- ```type```: Type of event, always ```extra``` for Extras event.
- ```extras.accountName```: Account Name of the user.
- ```extras.role```: Role of the user.
- ```extras.subgroup```: Subgroup of the user.
- ```extras.joinTime```: JoinTime of the user.
- ```extras.readyStatus```: Ready Status of the user.

Example of Extras event: 

```json
{
    "type": "extras",
    "extras":
    {
        "accountName": ":Knobin.5930",
        "role": 0,
        "subgroup": 1,
        "joinTime": 1656053181,
        "readyStatus": false
    }
}
```

## Other events

- [Combat](Combat.md) - ArcDPS events.
- [Squad](Squad.md) - Custom squad event.