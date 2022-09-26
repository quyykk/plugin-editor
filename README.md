# Editor

This is an unofficial plugin editor for Endless Sky. It is continuously updated, and supports the latest release of Endless Sky (including unstable "odd" versions).

Features:

- Supports galaxies, systems, planets, hazards, effects, fleets, governments, ships, outfits, shipyards, and outfitters.
- A powerful map editor!
- An outfitter to easily create ship loadouts.

## Screenshots

## Build instructions

Clone this repo with submodules:

```bash
$ git clone https://github.com/quyykk/plugin-editor --recursive
```

This guide assumes you have at least CMake 3.21.

Install the following, depending on your OS:

<details>
<summary>Windows</summary>
If you're planning on using Visual Studio, make sure to install the [clang/LLVM components](https://docs.microsoft.com/en-us/cpp/build/clang-support-msbuild) and the CMake component as well.

If you want to use MinGW (select the **MSVCRT runtime**; get it from [here](https://winlibs.com/#download-release)), I'd recommend using Visual Studio Code as IDE, because it provides pretty good CMake integration and MinGW support (including debugging).
</details>
<details>
<summary>MacOS</summary>
Install [Homebrew](https://brew.sh). Once it is installed, use it to install the tools you will need:

```
$ brew install cmake ninja pkg-config nasm
```
</details>
<details>
<summary>Debian distros</summary>
```
g++ cmake ninja-build pkg-config libgl1-mesa-dev libxmu-dev libxi-dev libglu1-mesa-dev tar zip unzip curl
```
</details>
<details>
<summary>Fedora distros</summary>
```
gcc-c++ cmake ninja-build mesa-libGL-devel autoconf libtool libXext-devel mesa-libGLU-devel
```
</details>

A lot of IDEs have built-in CMake integration (VS, VSCode, CLion, ...). For those, you can simply open the folder in the IDE and build.

There are a couple of presets to choose from before you build. Select the appropriate one. For example, if you want to build with MinGW select the `mingw` preset. (You can execute `cmake --preset help` to get a list of presets that are available).

If you want to build from the command line:

```bash
$ cmake --preset <preset>
$ cmake --build --preset <preset>-debug
```

where `<preset>` is your chosen preset. You can also specify `release` instead of `debug` to get a release build. The built files are located in the `build/` folder.
