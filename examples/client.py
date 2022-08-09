#
#  examples/client.py
#  ArcDPS Bridge
#
#  Created by Robin Gustafsson on 2022-06-22.
#


import win32pipe, win32file, pywintypes
import json
from enum import Enum


PipeName = r'\\.\pipe\arcdps-bridge'
Players = {}


class MessageCategory(Enum):
    Info    = 1,
    Combat  = 2,
    Extras  = 4,
    Squad   = 8


class MessageType(str, Enum):
    # Info types.
    BridgeInfo  = 1
    Status      = 2
    Closing     = 3

    # ArcDPS combat api types.
    CombatEvent = 4

    # Extras event types.
    ExtrasSquadUpdate       = 5
    ExtrasLanguageChanged   = 6
    ExtrasKeyBindChanged    = 7
    ExtrasChatMessage       = 8

    # Squad event types.
    SquadStatus     = 9
    SquadAdd        = 10
    SquadUpdate     = 11
    SquadRemove     = 12


def subscribe_message():
    # Event subscribe values: Combat = 2, Extras = 4, Squad = 8.
    # These values can be combined (or).
    # Protocols available: "JSON".
    sub = MessageCategory.Squad.value
    return '{"subscribe": ' + str(sub) + ', "protocol": "JSON"}'


def bridge_info(handle):
    # Read Bridge Info.
    result, data = win32file.ReadFile(handle, 64*1024)
    info = json.loads(data)
    print(f"Bride info: {info}")
    # Send subscription to Server.
    win32file.WriteFile(handle, str.encode(subscribe_message()))
    # Server will return a success message.
    result, msg = win32file.ReadFile(handle, 64*1024)
    status = json.loads(msg)
    print(f"Status: {status}")
    if status["data"]["success"] == False:
        err = status["data"]["error"]
        print(f"Server return with error msg: {err}")
    return status["data"]["success"]


def squad_message(msg_type, data):
    if (msg_type == MessageType.SquadStatus.name):
        # Initial Squad information.
        print("Self: ", data["self"])
        members = []
        for member in data["members"]:
            members.append(member["accountName"])
            Players[member["accountName"]] = member
        print("Squad members: ", members)
    if (msg_type == MessageType.SquadAdd.name):
        # Player has been added to squad.
        name = data["member"]["accountName"]
        print("Added \"", name, "\" to squad!")
        Players[name] = data["member"]
    if (msg_type == MessageType.SquadUpdate.name):
        # Player has been updated in squad.
        name = data["member"]["accountName"]
        updates = ""
        if name in Players:
            print("old ", Players[name])
            print("new ", data["member"])
            updates += player_diff(Players[name], data["member"])
            Players[name] = data["member"]
        else:
            print("Tried to update \"", name, "\", player not in squad.")
        print("Updated \"", name, "\" in squad with: {", updates, "}.")
    if (msg_type == MessageType.SquadRemove.name):
        # Player has been removed from squad.
        name = data["member"]["accountName"]
        print("Removed \"", name, "\" from squad!")
        if name in Players:
            del Players[name]
            if data["member"]["self"]:
                print("Self left, clearing squad...")
                Players.clear()


def compare_key(old, new, value):
    if old[value] != new[value]:
        return "\"" + value + "\": \"" + str(old[value]) + "\" => \"" + str(new[value]) + "\","
    return ""


def player_diff(old, new):
    updates = ""
    charName1 = "None" if old["characterName"] == None else old["characterName"]
    charName2 = "None" if new["characterName"] == None else new["characterName"]
    if charName1 != charName2:
        updates += "\"characterName\": \"" + charName1 + "\" => \"" + charName2 + "\","
    updates += compare_key(old, new, "joinTime")
    updates += compare_key(old, new, "profession")
    updates += compare_key(old, new, "elite")
    updates += compare_key(old, new, "role")
    updates += compare_key(old, new, "subgroup")
    updates += compare_key(old, new, "inInstance")
    updates += compare_key(old, new, "self")
    updates += compare_key(old, new, "readyStatus")
    return updates.removesuffix(",")


def pipe_client():
    print("Starting Pipe Client.")
    try:
        # Connect to pipe server.
        handle = win32file.CreateFile(
                PipeName,
                win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                0,
                None,
                win32file.OPEN_EXISTING,
                0,
                None
            )
        res = win32pipe.SetNamedPipeHandleState(handle, win32pipe.PIPE_READMODE_MESSAGE, None, None)
        if res == 0:
            print(f"SetNamedPipeHandleState return code: {res}")
        
        # Pipe is connected to server.
        # Server will send Bridge Information, and expects a subscribe return.
        if bridge_info(handle):
            # Subscribed to events successfully.
            # Server will now send events to client.
            while True:
                result, msg = win32file.ReadFile(handle, 64*1024)
                evt = json.loads(msg)

                if (evt["category"] == MessageCategory.Squad.name):
                    squad_message(evt["type"], evt["data"])

                if (evt["category"] == MessageCategory.Info.name):
                    if (evt["type"] == MessageType.BridgeInfo.name):
                        # New bridge information is available.
                        binfo = evt["data"]
                        print(f"New bridge informaion received: {binfo}")
                    elif (evt["type"] == MessageType.Closing.name):
                        # Server is closing here, no further messages will be sent.
                        print("Server is closing.")
                        break;

    except pywintypes.error as e:
        print("Exception caught: ", str(e))
        if e.args[0] == 2:
            print("No pipe!")
        elif e.args[0] == 109:
            print("Pipe is broken!")
    print("Ended Pipe Client.")


if __name__ == '__main__':
    pipe_client();