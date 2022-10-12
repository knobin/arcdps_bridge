# arcdps_bridge

An ArcDPS Extension which sends the events over a named pipe and supports both ArcDPS and ArcDPS Unofficial Extras events.

## Features

- Clients can receive full ArcDPS events, and ArcDPS Unofficial Extras events. Or access squad information through a custom "Squad" event.
- "Squad" events information: 
    - Players joining or leaving the squad.
    - Instance information, players in or out of the instance.
    - Character information: character name, profession and elite.
    - Composition changes: character information and subgroup changes.
- Internally keeps track of the squad and client will receive squad information when connected.
- Supports any programming language that can interface with Windows Named Pipe and parse JSON.
- Clients can chose to receive messages in either JSON or Serial format. 
- Multiple clients can be connected at the same time.

### Limitations

- Character information (character name, profession and elite) will only be updated when player is in the same instance and the player is the source of an ArcDPS combat event (boon applied to player for example).
- Game crashing and when the player is in a squad will result in the internal squad information being removed. Starting the game again will begin rebuilding it and will be fully rebuilt when all players in the squad have entered or been in the same instance.

## Why?

This extension was mainly created for being able to track squad members and send that information to an external application with minimal limitations as possible. Therefore, the extension keeps track of the squad members internally so the information is always as up-to-date and complete as possible. There are still some [limitations](#Limitations) but they are very hard to overcome. Over time the extension also gained the ability to send ArcDPS and Unofficial Extras events as well in case the client needed them. And at the time of creation there was no extension that could send both of these only combat events, this has now changed.

### Why should I use this?

You could use this extension if you:

- Have an external application and want to parse ArcDPS or Unofficial Extras events.
- Need to have complete information about the squad at any time (application launches after gw2 for example).

If you are creating an extension for ArcDPS and want to parse the events you should not use this extension. You should subscribe to ArcDPS and Unofficial Extras yourself. As it makes very little sense to introduce the pipe overhead and serialization.

## Getting Started

Follow the [installation](#installation) process, then have a look at the [documentation](docs) to get familiar with the API.

For example clients see: [Examples](examples)

## Installation

Requires [ArcDPS](https://www.deltaconnected.com/arcdps/), and [ArcDPS Unofficial Extras](https://github.com/Krappa322/arcdps_unofficial_extras_releases).

Latest release is available at [arcdps_bridge.dll](https://github.com/knobin/arcdps_bridge/releases/latest). Drag and drop that DLL in the same directory as ArcDPS is located in.

It can also be built with the following instructions below:

### Building

Start by cloning the repository:

```shell
$ git clone --recursive https://github.com/knobin/arcdps_bridge.git
```

Step into the repository, and create a build directory then enter it:

```shell
$ cd arcdps_bridge
$ mkdir build
$ cd build
```

Building requires CMake with minimum version 3.12.4 and Visual Studio 17.

```shell
$ cmake -DCMAKE_BUILD_TYPE=Release -G "Visual Studio 17 2022" -A x64  ..
$ cmake --build . --config Release
```

The built DLL is now located at 

```shell
Release/arcdps_bridge.dll
```

Now drag and drop that DLL in the same directory as ArcDPS is located in.

## License
This project is licensed with the [MIT License](LICENSE).

This project uses third-party dependencies:
* [ArcDPS Unofficial Extras](https://github.com/Krappa322/arcdps_unofficial_extras_releases): Closed source addon where the public API that is used in this project is licensed with the [MIT License](https://github.com/Krappa322/arcdps_unofficial_extras_releases/blob/master/LICENSE).
* [Catch2](https://github.com/catchorg/Catch2): A unit testing framework for C++ licensed with the [MIT License](https://github.com/catchorg/Catch2/blob/devel/LICENSE.txt)
* [nlohmann_json](https://github.com/nlohmann/json): JSON for Modern C++ licensed with the [MIT License](https://github.com/nlohmann/json/blob/develop/LICENSE.MIT).
* [spdlog](https://github.com/gabime/spdlog): Fast C++ logging library licensed with the [MIT License](https://github.com/gabime/spdlog/blob/v1.x/LICENSE).