#
#  examples/client.py
#  ArcDPS Bridge
#
#  Created by Robin Gustafsson on 2022-06-22.
#


import win32pipe, win32file, pywintypes
import json


PipeName = r'\\.\pipe\arcdps-bridge'
Players = {}

def subscribe_message():
    # Event subscribe values: Combat = 1, Extras = 2, Squad = 4.
    # These values can be combined (or):
    return '{"subscribe":"4"}'


def bridge_info(handle):
    # Read Bridge Info.
    result, data = win32file.ReadFile(handle, 64*1024)
    info = json.loads(data)
    print(f"Bride info: {info}")
    # Send subscription to Server.
    win32file.WriteFile(handle, str.encode(subscribe_message()))
    # Server will send a success message.
    result, msg = win32file.ReadFile(handle, 64*1024)
    data = json.loads(msg)
    if data["status"]["success"] == False:
        err = data["status"]["error"]
        print(f"Server return with error msg: {err}")
    return data["status"]["success"]


def squad_message(data):
    if (data["trigger"] == "status"):
        # Initial Squad information.
        print("Self: ", data["status"]["self"])
        members = []
        for member in data["status"]["members"]:
            members.append(member["accountName"])
            Players[member["accountName"]] = member
        print("Squad members: ", members)
    if (data["trigger"] == "add"):
        # Player has been added to squad.
        name = data["add"]["member"]["accountName"]
        print("Added \"", name, "\" to squad!")
        Players[name] = data["add"]["member"]
    if (data["trigger"] == "update"):
        # Player has been updated in squad.
        name = data["update"]["member"]["accountName"]
        updates = ""
        if name in Players:
            print("old ", Players[name])
            print("new ", data["update"]["member"])
            updates += player_diff(Players[name], data["update"]["member"])
            Players[name] = data["update"]["member"]
        else:
            print("Tried to update \"", name, "\", player not in squad.")
        print("Updated \"", name, "\" in squad with: {", updates, "}.")
    if (data["trigger"] == "remove"):
        # Player has been removed from squad.
        name = data["remove"]["member"]["accountName"]
        print("Removed \"", name, "\" from squad!")
        if name in Players:
            del Players[name]
            if data["remove"]["member"]["self"]:
                print("Clearing squad..")
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
                data = json.loads(msg)
                if (data["type"] == "squad"):
                    squad_message(data["squad"])
                if (data["type"] == "closing"):
                    print("Server is closing.")
                    break
                if (data["type"] == "info"):
                    # New bridge information is available.
                    print(f"New bridge informaion received: {data}")

    except pywintypes.error as e:
        print("Exception caught: ", str(e))
        if e.args[0] == 2:
            print("No pipe!")
        elif e.args[0] == 109:
            print("Pipe is broken!")
    print("Ended Pipe Client.")


if __name__ == '__main__':
    pipe_client();