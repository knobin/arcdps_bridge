# arcdps_bridge

An ArcDPS Extension which sends the events over a named pipe.

## Features

- Clients can recieve full ArcDPS events, and ArcDPS Unofficial Extras events. Or access squad information through a custom "Squad" event.
- "Squad" events information: 
    - Players joining or leaving the squad.
    - Instance information, players in or out of the instance.
    - Character information: character name, profession and elite.
    - Composition changes: character information and subgroup changes.
- Internally keeps track of the squad and client will receive squad information when connected.
- Supports any programming language that can interface with Windows Named Pipe and parse JSON.
- Multiple clients can be connected at the same time.

### Limitations

- Character information (character name, profession and elite) will only be updated when player is in the same instance.
- Game crashing when in a squad will cause the internal squad information to be removed, it will be fully rebuilt when all players in the squad have been entered the same instance.

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
* [spdlog](https://github.com/gabime/spdlog): Fast C++ logging library licensed with the [MIT License](https://github.com/gabime/spdlog/blob/v1.x/LICENSE).