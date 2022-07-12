# Events

Common information in all of the events.

## Table of Contents

- [Common information](#common-information)
- [Specific events](#specific-events)

## Common information

All events are JSON objects, and will have a ```type``` string variable identifying what type of event that it is. For example: ```squad``` for squad event, ```combat``` for ArcDPS event, and ```extras``` for ArcDPS Unofficial Extras event.

This also describes where the data from the event is located at. For example: ```squad``` type will have data in the ```squad``` member.

For example:

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
                    "role": 0,
                    "subgroup": 1,
                    "inInstance": true
                }
            ]
        }
    }
}
```

## Specific events

- [Combat](Combat.md) - ArcDPS events.
- [Extras](Extras.md) - ArcDPS Unofficial Extras events.
- [Squad](Squad.md) - Custom squad event.