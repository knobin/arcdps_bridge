# Squad events

Squad events are derived from both [Combat](Combat.md), and [Extras](Extras.md) events that describes information, and changes in the squad.

## Table of Contents

- [PlayerCollection](#playercollection)
    - [Combat vs Extras event details](#combat-vs-extras-event-details)
- [Squad event details](#squad-event-details)
    - [Status](#status)
    - [Add](#add)
    - [Remove](#remove)
    - [Update](#update)
- [Other events](#other-events)


## PlayerCollection

The extension keeps track of the squad composition without any clients listening for events. This means that the client can connect at any time and receive up-to-date squad information. Although, since the squad is built up from [Combat](Combat.md), and [Extras](Extras.md) means that it can miss some information like the character name of a player. This will be fixed when a combat event is found, that updates the squad composition. This also generates an [update event](#update).

Another issue arises when the game is restarted (or crashed), then the squad composition will be lost, while the player is still in the squad when starting the game. However, the squad will be rebuilt by combat events in time when the members enter the same instance as the player.

### Combat vs Extras event details

Since the combat and extras event are a bit different, there may be some fields that are invalid when received, and how they will update the internal PlayerCollection. The combat event will update ```accountName```, ```characterName```, ```profession```, ```elite```, and ```subgroup```. Leaving out the ```joinTime``` and ```role```. On the other hand, extras event will update ```accountName```, ```role```, ```subgroup``` and ```joinTime```. Leaving out ```characterName```, ```profession``` and ```elite```. 

Although, if ```characterName``` is not null, the ```profession``` and ```elite``` will also be valid. To check what fields are guaranteed to be valid when received, you can check the ```squad.add.source``` and see what event that caused the event. Those fields will be updated, this does not mean however that the other fields are invalid, simply not just updated, and they may or may not be valid.

## Squad event details

All squad events include at least:

- ```type```: Type of event, always ```squad``` for squad events.
- ```squad.trigger```: The cause of the creation, can be ```status```, ```add```, ```remove```, or ```update```. Also determines the data location. For example, the "status" trigger will have its data in ```squad.status```.

If the event includes player information the object will include:

- ```accountName```: Account name of the player (```combat``` and ```extras```).
- ```characterName```: Character name of the player (```combat```).
- ```joinTime```: JoinTime of the player (```extras```).
- ```profession```: Profession of the character (```combat```).
- ```elite```: Elite of the character (```combat```).
- ```role```: Role of the user (```extras```).
- ```subgroup```: Subgroup of the user (```combat``` and ```extras```).

Example of player information object: 

```json
{
    "accountName": ":Knobin.5930",
    "characterName": "Knobin",
    "joinTime": 0,
    "profession": 4,
    "elite": 55,
    "role": 1,
    "subgroup": 1
}
```

### Status

When a client has connected, and subscribed to squad events. The squad status event will be sent informing the client about the current squad composition.

The event includes:
- ```type```: Type of event, always ```squad``` for squad events.
- ```squad.trigger```: The cause of the creation, always ```status``` for squad status events.
- ```squad.status.self```: Account name of the player.
- ```squad.status.members```: Current squad members, array of player information objects.

Example of squad status event: 

```json
{
    "type": "squad",
    "squad":
    {
        "trigger": "status",
        "status":
        {
            "self": ":Knobin.5930",
            "members": [
                {
                    "accountName": ":Knobin.5930",
                    "characterName": "Knobin",
                    "joinTime": 0,
                    "profession": 4,
                    "elite": 55,
                    "role": 1,
                    "subgroup": 1
                }
            ]
        }
    }
}
```

### Add

Squad event that is sent when a player is added to the squad, through either ```combat``` or ```extras``` events.

The event includes:
- ```type```: Type of event, always ```squad``` for squad events.
- ```squad.trigger```: The cause of the creation, always ```add``` for squad add events.
- ```squad.add.source```: Indicates from what event source this was generated from. Can either be ```combat``` or ```extras```.
- ```squad.add.member```: Added squad member, player information object.

Example of squad add event with "extras" source:

```json
{
    "type": "squad",
    "squad":
    {
        "trigger": "add",
        "add":
        {
            "source": "extras",
            "member":
            {
                "accountName": ":Knobin.5930",
                "characterName": null,
                "joinTime": 1656069715,
                "profession": 0,
                "elite": 0,
                "role": 0,
                "subgroup": 1
            }
        }
    }
}
```

Example of squad add event with "combat" source:

```json
{
    "type": "squad",
    "squad":
    {
        "trigger": "add",
        "add":
        {
            "source": "combat",
            "member":
            {
                "accountName": ":Knobin.5930",
                "characterName": "Knobin",
                "joinTime": 0,
                "profession": 4,
                "elite": 55,
                "role": 0,
                "subgroup": 1
            }
        }
    }
}
```

### Remove

Squad event that is sent when a player is removed from the squad, only generated from ```extras``` events.

The event includes:
- ```type```: Type of event, always ```squad``` for squad events.
- ```squad.trigger```: The cause of the creation, always ```remove``` for squad remove events.
- ```squad.remove.source```: Indicates from what event source this was generated from. Can only be ```extras```.
- ```squad.remove.member```: Removed squad member, player information object.

Example of squad remove event:

```json
{
    "type": "squad",
    "squad":
    {
        "trigger": "remove",
        "remove":
        {
            "source": "extras",
            "member":
            {
                "accountName": ":Knobin.5930",
                "characterName": "Knobin",
                "joinTime": 0,
                "profession": 4,
                "elite": 55,
                "role": 0,
                "subgroup": 1
            }
        }
    }
}
```

### Update

Squad event that is sent when a player is updated in the internal PlayerCollection, through either ```combat``` or ```extras``` events.

The event includes:
- ```type```: Type of event, always ```squad``` for squad events.
- ```squad.trigger```: The cause of the creation, always ```update``` for squad update events.
- ```squad.update.source```: Indicates from what event source this was generated from. Can either be ```combat``` or ```extras```.
- ```squad.update.member```: Updated squad member, player information object.

Example of squad update event with "extras" source:

```json
{
    "type": "squad",
    "squad":
    {
        "trigger": "update",
        "update":
        {
            "source": "extras",
            "member":
            {
                "accountName": ":Knobin.5930",
                "characterName": "Knobin",
                "joinTime": 0,
                "profession": 4,
                "elite": 55,
                "role": 0,
                "subgroup": 1
            }
        }
    }
}
```

## Other events

- [Combat](Combat.md) - ArcDPS events.
- [Extras](Extras.md) - ArcDPS Unofficial Extras events.
