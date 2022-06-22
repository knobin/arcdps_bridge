# arcdps_bridge

An ArcDPS Extension which sends the events over a named pipe.

## Features

- Squad information: Players joining or leaving, character information, and subgroup changes.
- Clients can recieve full ArcDPS events, and ArcDPS Unofficial Extras events. Or access squad information through a custom "Squad" event.
- Supports any programming language that can interface with Windows NamedPipe, and parse JSON.
- Multiple clients can be connected at the same time.

For example clients see:
* [Examples](examples)

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