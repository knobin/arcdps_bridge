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

Since the combat and extras event are a bit different, there may be some fields that are invalid when received, and how they will update the internal PlayerCollection. The combat event will update ```accountName```, ```characterName```, ```profession```, ```elite```, ```subgroup``` and ```inInstance```. Leaving out the ```joinTime``` and ```role```. On the other hand, extras event will update ```accountName```, ```role```, ```subgroup``` and ```joinTime```. Leaving out ```characterName```, ```profession``` and ```elite```. 

Although, if ```characterName``` is not null, the ```profession``` and ```elite``` will also be valid. Another way is to check ```inInstance``` if it is true, this means that ```characterName```, ```profession``` and ```elite``` are all valid and up-to-date. To check what fields are guaranteed to be valid when received, you can check the ```squad.add.source``` and see what event that caused the event. Those fields will be updated, this does not mean however that the other fields are invalid, simply not just updated, and they may or may not be valid.

## Squad event details

All squad events include at least:

- ```type```: Type of event, always ```squad``` for squad events.
- ```squad.trigger```: The cause of the creation, can be ```status```, ```add```, ```remove```, or ```update```. Also determines the data location. For example, the "status" trigger will have its data in ```squad.status```.

If the event includes player information the object will include:

- ```accountName```: Account name of the player (```combat``` and ```extras```).
- ```characterName```: Character name of the player (```combat```), ```null``` if there is no character name available.
- ```joinTime```: JoinTime of the player (```extras```), ```0``` if there is no join time available.
- ```profession```: Profession of the character (```combat```), ```0``` if there is no profession available.
- ```elite```: Elite of the character (```combat```), ```0``` if there is no elite available.
- ```role```: Role of the user (```extras```).
- ```subgroup```: Subgroup of the user (```combat``` and ```extras```).
- ```self```: Is the user self (```combat``` and ```extras```).
- ```inInstance```: Is the user in the instance (```combat```).
- ```readyStatus```: Squad ready check (```true``` or ```false```).

The parentheses describes where the value is retrieved from, if there are multiple origins the latest value is used.

Example of player information object: 

```json
{
    "accountName": ":Knobin.5930",
    "characterName": "Knobin",
    "joinTime": 1658850989,
    "profession": 4,
    "elite": 55,
    "role": 2,
    "subgroup": 1,
    "self": true,
    "inInstance": true,
    "readyStatus": false
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
                    "joinTime": 1658850989,
                    "profession": 4,
                    "elite": 55,
                    "role": 2,
                    "subgroup": 1,
                    "self": true,
                    "inInstance": true,
                    "readyStatus": false
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
- ```squad.add.source```: Indicates from what event source this event was caused by. Can either be ```combat``` or ```extras```.
- ```squad.add.validator```: Indicates what version of member information this is, always ```1``` for add events.
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
            "validator": 1,
            "member":
            {
                "accountName": ":Knobin.5930",
                "characterName": "Knobin",
                "joinTime": 1658850989,
                "profession": 4,
                "elite": 55,
                "role": 2,
                "subgroup": 1,
                "self": true,
                "inInstance": true,
                "readyStatus": false
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
            "validator": 1,
            "member":
            {
                "accountName": ":Knobin.5930",
                "characterName": "Knobin",
                "joinTime": 0,
                "profession": 4,
                "elite": 55,
                "role": 0,
                "subgroup": 1,
                "self": true,
                "inInstance": true,
                "readyStatus": false
            }
        }
    }
}
```

Just because the source might be "combat" or "extras" does not mean that it does not contain the combined data. The two examples above shows the differens when there is no data recorded yet. If there is a "extras" event already recorded, it will add the new data from the "combat" event and then trigger this event with the combined data with the source still being "combat".

### Remove

Squad event that is sent when a player is removed from the squad, through either ```combat``` or ```extras``` events.

The event includes:
- ```type```: Type of event, always ```squad``` for squad events.
- ```squad.trigger```: The cause of the creation, always ```remove``` for squad remove events.
- ```squad.remove.source```: Indicates from what event source this event was caused by. Can either be ```combat``` or ```extras```. 
- ```squad.remove.validator```: Indicates what version of member information this is (latest add or update validator).
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
            "validator": 4,
            "member":
            {
                "accountName": ":Knobin.5930",
                "characterName": "Knobin",
                "joinTime": 0,
                "profession": 4,
                "elite": 55,
                "role": 2,
                "subgroup": 1,
                "self": true,
                "inInstance": true,
                "readyStatus": false
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
- ```squad.update.source```: Indicates from what event source this event was caused by. Can either be ```combat``` or ```extras```. 
- ```squad.update.validator```: Indicates what version of member information this is (higher is newer).
- ```squad.update.member```: Updated squad member, player information object.

Example of squad update event with "combat" source:

```json
{
    "type": "squad",
    "squad":
    {
        "trigger": "update",
        "update":
        {
            "source": "combat",
            "validator": 2,
            "member":
            {
                "accountName": ":Knobin.5930",
                "characterName": "Knobin",
                "joinTime": 1658850989,
                "profession": 4,
                "elite": 5,
                "role": 2,
                "subgroup": 1,
                "self": true,
                "inInstance": true,
                "readyStatus": false
            }
        }
    }
}
```

## Other events

- [Combat](Combat.md) - ArcDPS events.
- [Extras](Extras.md) - ArcDPS Unofficial Extras events.
