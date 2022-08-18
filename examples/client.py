#
#  examples/client.py
#  ArcDPS Bridge
#
#  Created by Robin Gustafsson on 2022-06-22.
#


from email.message import Message
from unicodedata import category
import win32pipe, win32file, pywintypes
import json

from enum import Enum

from rich.console import Console
from rich.layout import Layout


PipeName = r'\\.\pipe\arcdps-bridge'
Players = {}

console = Console()
layout = Layout()


###################################################################################################

#
# Interface enums.
#


class MessageCategory(Enum):
    Info    = 1
    Combat  = 2
    Extras  = 4
    Squad   = 8


class MessageType(Enum):
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


class MessageProtocol(Enum):
    Serial  = 1
    JSON    = 2


###################################################################################################


#
# JSON interface.
#


def subscribe_message(sub, protocol):
    return '{"subscribe": ' + str(sub) + ', "protocol": "' + protocol.name + '"}'


def setup_connection(handle, sub_msg):
    # Read Bridge Info.
    result, data = win32file.ReadFile(handle, 64*1024)
    info = json.loads(data)
    print(f"Bridge info: {info}")
    # Send subscription to Server.
    win32file.WriteFile(handle, str.encode(sub_msg))
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
            members.append(member["player"]["accountName"])
            Players[member["player"]["accountName"]] = member["player"]
        print("Squad members: ", members)
    if (msg_type == MessageType.SquadAdd.name):
        # Player has been added to squad.
        name = data["player"]["accountName"]
        print("Added \"", name, "\" to squad!")
        Players[name] = data["player"]
    if (msg_type == MessageType.SquadUpdate.name):
        # Player has been updated in squad.
        name = data["player"]["accountName"]
        updates = ""
        if name in Players:
            print("old ", Players[name])
            print("new ", data["player"])
            updates += player_diff(Players[name], data["player"])
            Players[name] = data["player"]
        else:
            print("Tried to update \"", name, "\", player not in squad.")
        print("Updated \"", name, "\" in squad with: {", updates, "}.")
    if (msg_type == MessageType.SquadRemove.name):
        # Player has been removed from squad.
        name = data["player"]["accountName"]
        print("Removed \"", name, "\" from squad!")
        if name in Players:
            del Players[name]
            if data["player"]["self"]:
                print("Self left, clearing squad...")
                Players.clear()


def compare_key(old, new, value):
    if old[value] != new[value]:
        oval = str(old[value])
        nval = str(new[value])
        if (isinstance(old[value], str)):
            oval = "\"" + oval + "\""
        if (isinstance(new[value], str)):
            nval = "\"" + nval + "\""
        return "\"" + value + "\": " + oval + " => " + nval + ", "
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
    return updates.removesuffix(", ")


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
        sub_value = MessageCategory.Combat.value | MessageCategory.Squad.value
        if setup_connection(handle, subscribe_message(sub_value, MessageProtocol.JSON)):
            # Subscribed to events successfully.
            # Server will now send events to client.
            while True:
                result, msg = win32file.ReadFile(handle, 64*1024)
                evt = json.loads(msg)
                print(evt);

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


###################################################################################################


#
# Serial interface.
#


def MessageCategory_from_uint8(value):
    try:
        category = MessageCategory(value)
        return category
    except:
        return None


def MessageType_from_uint8(value):
    try:
        msgtype = MessageType(value)
        return msgtype
    except:
        return None


def read_string_len(data):
    index = 0
    for byte in data:
        index = index + 1
        if byte == 0:
            break
    return index


def read_string(data):
    d = dict()
    d["count"] = read_string_len(data) # inlcudes null terminator.
    d["value"] = data[: d["count"]-1].decode("utf-8")
    return d


def read_integer_from_bytes(data, count, is_signed):
    return int.from_bytes(data[:count], byteorder='little', signed=is_signed)


def read_int(data, count):
    return read_integer_from_bytes(data, count, True)


def read_uint(data, count):
    return read_integer_from_bytes(data, count, False)


def read_player(data):
    acc = read_string(data)
    acc_len = acc["count"]

    char = read_string(data[acc_len:])
    char_len = char["count"]

    offset = acc_len + char_len

    d = dict()

    d["value"] = {
        "accountName": acc["value"],
        "characterName": char["value"],
        "joinTime": read_int(data[offset:], 8), # int.from_bytes(data[offset : offset+8], byteorder='little', signed=True),
        "profession": read_uint(data[offset+8:], 4), # int.from_bytes(data[offset+8 : offset+8+4], byteorder='little', signed=False),
        "elite": read_uint(data[offset+8+4:], 4), # int.from_bytes(data[offset+8+4 : offset+8+4+4], byteorder='little', signed=False),
        "role": read_uint(data[offset+8+4+4:], 1), # int.from_bytes(data[offset+8+4+4 : offset+8+4+4+1], byteorder='little', signed=False),
        "subgroup": read_uint(data[offset+8+4+4+1:], 1), # int.from_bytes(data[offset+8+4+4+1 : offset+8+4+4+1+1], byteorder='little', signed=False),
        "inInstance": bool(read_uint(data[offset+8+4+4+1+1:], 1)), # bool(int.from_bytes(data[offset+8+4+4+1+1 : offset+8+4+4+1+1+1], byteorder='little', signed=False)),
        "self": bool(read_uint(data[offset+8+4+4+1+1+1:], 1)), # bool(int.from_bytes(data[offset+8+4+4+1+1+1 : offset+8+4+4+1+1+1+1], byteorder='little', signed=False)),
        "readyStatus": bool(read_uint(data[offset+8+4+4+1+1+1+1:], 1)), #  bool(int.from_bytes(data[offset+8+4+4+1+1+1+1 : offset+8+4+4+1+1+1+1+1], byteorder='little', signed=False))
    }
    d["count"] = offset+8+4+4+1+1+1+1+1
    return d


def read_playerentry(data):
    d = dict()

    member = read_player(data)

    d["value"] = {
        "member": member["value"],
        "validator": int.from_bytes(data[member["count"] : member["count"]+8], byteorder='little', signed=False)
    }
    d["count"] = member["count"] + 8
    return d


def read_squadstatus(data):
    self_len = read_string_len(data)
    self_str = data[: self_len - 1].decode("utf-8")
    offset = self_len
    print("Self: ", self_str)

    count = read_uint(data[offset:], 1)
    offset = offset + 8
    print("Count: ", count)

    for i in range(count):
        entry = read_playerentry(data[offset:])
        offset = offset + entry["count"]
        print(entry)


def read_bridgeinfo(data):
    version = read_string(data)
    offset = version["count"]

    extras_version = read_string(data[offset:])
    offset = offset + extras_version["count"]

    arc_version = read_string(data[offset:])
    offset = offset + arc_version["count"]

    validator = read_int(data[offset:], 8, True)
    offset = offset + 8

    info = {
        "version": version["value"], 
        "extrasVersion": extras_version["value"],
        "arcvers": arc_version["value"],
        "validator": validator,
        "arcLoaded": bool(read_uint(data[offset:], 1)),
        "extrasFound": bool(read_uint(data[offset+1:], 1)),
        "extrasLoaded": bool(read_uint(data[offset+1+1:], 1))
    }
    print("New Bridge information (serial): ", info)


def read_ag(data):
    name = read_string(data)
    offset = name["count"]

    d = dict()
    d["value"] = {
        "name": name["value"],
        "id": read_uint(data[offset:], 8),
        "prof": read_uint(data[offset+8:], 4),
        "elite": read_uint(data[offset+8+4:], 4),
        "self": read_uint(data[offset+8+4+4:], 4),
        "team": read_uint(data[offset+8+4+4+4:], 2)
    }
    d["count"] = offset+8+4+4+4+2
    return d


def read_cbtevent(data):
    to_read = [
        {"name": "time",                "count": 8, "is_signed": False},
        {"name": "src_agent",           "count": 8, "is_signed": False},
        {"name": "dst_agent",           "count": 8, "is_signed": False},
        {"name": "value",               "count": 4, "is_signed": True},
        {"name": "buff_dmg",            "count": 4, "is_signed": True},
        {"name": "overstack_value",     "count": 4, "is_signed": False},
        {"name": "skillid",             "count": 4, "is_signed": False},
        {"name": "src_instid",          "count": 2, "is_signed": False},
        {"name": "dst_instid",          "count": 2, "is_signed": False},
        {"name": "src_master_instid",   "count": 2, "is_signed": False},
        {"name": "dst_master_instid",   "count": 2, "is_signed": False},
        {"name": "iff",                 "count": 1, "is_signed": False},
        {"name": "buff",                "count": 1, "is_signed": False},
        {"name": "result",              "count": 1, "is_signed": False},
        {"name": "is_activation",       "count": 1, "is_signed": False},
        {"name": "is_buffremove",       "count": 1, "is_signed": False},
        {"name": "is_ninety",           "count": 1, "is_signed": False},
        {"name": "is_fifty",            "count": 1, "is_signed": False},
        {"name": "is_moving",           "count": 1, "is_signed": False},
        {"name": "is_statechange",      "count": 1, "is_signed": False},
        {"name": "is_flanking",         "count": 1, "is_signed": False},
        {"name": "is_shields",          "count": 1, "is_signed": False},
        {"name": "is_offcycle",         "count": 1, "is_signed": False}
    ]

    offset = 0
    values = dict()
    for x in to_read:
        xcount = x["count"]
        values[x["name"]] = read_integer_from_bytes(data[offset:], xcount, x["is_signed"])
        offset = offset + xcount
    
    return {"value": values, "count": offset}
    

def read_combat(data):
    non_null_bits = read_uint(data, 1)
    offset = 1

    ev_exist = bool(non_null_bits & 1)
    src_exist = bool(non_null_bits & 2)
    dst_exist = bool(non_null_bits & 4)

    ev = None
    src = None
    dst = None

    if ev_exist:
        ev_data = read_cbtevent(data[offset:])
        ev = ev_data["value"]
        offset = offset + ev_data["count"]

    if src_exist:
        src_data = read_ag(data[offset:])
        src = src_data["value"]
        offset = offset + src_data["count"]

    if dst_exist:
        dst_data = read_ag(data[offset:])
        dst = dst_data["value"]
        offset = offset + dst_data["count"]

    skillname = read_string(data[offset:])
    offset = offset + skillname["count"]

    d = dict()
    d["value"] = {
        "ev": ev,
        "src": src,
        "dst": dst,
        "skillname": skillname["value"],
        "id": read_uint(data[offset:], 8),
        "revision": read_uint(data[offset+8:], 8)
    }
    d["count"] = offset+8+8
    return d


def read_squadextras(data):
    account_name = read_string(data)
    offset = account_name["count"]
    
    d = dict()
    d["value"] = {
        "AccountName": account_name["value"],
        "JoinTime": read_int(data[offset:], 8),
        "Role": read_uint(data[offset+8:], 1),
        "Subgroup": read_uint(data[offset+8+1:], 1),
        "ReadyStatus": read_uint(data[offset+8+1+1:], 1)
    }
    d["count"] = offset+8+1+1+1
    return d


def info_serial(msgtype, data):
    print("info_serial", msgtype)
    if msgtype == MessageType.BridgeInfo:
        read_bridgeinfo(data)
        pass
    elif msgtype == MessageType.Status:
        # Status event is only received in json form!
        pass
    elif msgtype == MessageType.Closing:
        # Closing is handled outside of this function.
        # The event doesn't contain any data either.
        pass


def combat_serial(msgtype, data):
    print("combat_serial", msgtype)
    combat = read_combat(data)
    print("Combat event: ", combat)


def extras_serial(msgtype, data):
    print("extras_serial", msgtype)
    if msgtype == MessageType.ExtrasSquadUpdate:
        print("Received SquadStatus")
        extras = read_squadextras(data)
        print(extras)
    elif msgtype == MessageType.ExtrasLanguageChanged:
        # TODO
        pass
    elif msgtype == MessageType.ExtrasKeyBindChanged:
        # TODO
        pass
    elif msgtype == MessageType.ExtrasChatMessage:
        # TODO
        pass

def squad_serial(msgtype, data):
    if msgtype == MessageType.SquadStatus:
        print("Received SquadStatus")
        read_squadstatus(data)
    elif msgtype == MessageType.SquadAdd:
        print("Received SquadAdd")
        entry = read_playerentry(data)
        print(entry)
    elif msgtype == MessageType.SquadUpdate:
        print("Received SquadUpdate")
        entry = read_playerentry(data)
        print(entry)
    elif msgtype == MessageType.SquadRemove:
        print("Received SquadRemove")
        entry = read_playerentry(data)
        print(entry)


def pipe_client_serial():
    print("Starting Pipe Client (serial).")
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
        sub_value = MessageCategory.Extras.value #  MessageCategory.Combat.value | MessageCategory.Squad.value 
        if setup_connection(handle, subscribe_message(sub_value, MessageProtocol.Serial)):
            # Subscribed to events successfully.
            # Server will now send events to client.
            while True:
                result, data_bytes = win32file.ReadFile(handle, 64*1024)
                print(f"Data received: {''.join('{:02X} '.format(c) for c in data_bytes)}")

                if len(data_bytes) > 1:
                    category = MessageCategory_from_uint8(read_uint(data_bytes, 1))
                    msgtype = MessageType_from_uint8(read_uint(data_bytes[1:], 1))
                    print(category, msgtype, read_uint(data_bytes, 1), read_uint(data_bytes[1:], 1))
                    if category != None and msgtype != None:
                        if category == MessageCategory.Info:
                            if msgtype == MessageType.Closing:
                                # Server is closing here, no further messages will be sent.
                                print("Server is closing.")
                                break;
                            else:
                                info_serial(msgtype, data_bytes[2:])
                        elif category == MessageCategory.Combat:
                            combat_serial(msgtype, data_bytes[2:])
                        elif category == MessageCategory.Extras:
                            extras_serial(msgtype, data_bytes[2:])
                        elif category == MessageCategory.Squad:
                            squad_serial(msgtype, data_bytes[2:])
    
    except pywintypes.error as e:
        print("Exception caught: ", str(e))
        if e.args[0] == 2:
            print("No pipe!")
        elif e.args[0] == 109:
            print("Pipe is broken!")
    print("Ended Pipe Client.")


###################################################################################################


#
# Main.
#


if __name__ == '__main__':
    #  console.print(layout)
    #pipe_client();
    pipe_client_serial()