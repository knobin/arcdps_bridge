import time
import sys
import win32pipe, win32file, pywintypes
import json


PipeName = r'\\.\pipe\arcdps-bridge'


def subscribe_message():
    # Event subscribe values: Combat = 1, Extra = 2, Squad = 4.
    # These values can be combined (or):
    return '{"subscribe":"4"}'


def bridge_info(handle):
    # Read Bridge Info.
    result, data = win32file.ReadFile(handle, 64*1024)
    info = json.loads(data)
    print(f"Bride info: {info}")
    # Send subscription to Server.
    win32file.WriteFile(handle, str.encode(subscribe_message()))


def squad_message(data):
    if (data["trigger"] == "status"):
        # Initial Squad information.
        print("Self: ", data["self"])
        members = []
        for member in data["members"]:
            members.append(member["AccountName"])
        print("Squad members: ", members)
    if (data["trigger"] == "add"):
        # Player has been added to squad.
        print("Added \"", data["member"]["AccountName"], "\" to squad!")
    if (data["trigger"] == "update"):
        # Player has been updated in squad.
        print("Updated \"", data["member"]["AccountName"], "\" in squad!")
    if (data["trigger"] == "remove"):
        # Player has been removed from squad.
        print("Removed \"", data["member"]["AccountName"], "\" from squad!")


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
        bridge_info(handle)

        # Server will now send events to client.
        while True:
            result, msg = win32file.ReadFile(handle, 64*1024)
            data = json.loads(msg)
            if (data["type"] == "Squad"):
                squad_message(data["squad"])

    except pywintypes.error as e:
        print("Exception caught: ", str(e))
        if e.args[0] == 2:
            print("No pipe!")
            time.sleep(1)
        elif e.args[0] == 109:
            print("Pipe is broken!")
    print("Ended Pipe Client.")


if __name__ == '__main__':
    pipe_client();