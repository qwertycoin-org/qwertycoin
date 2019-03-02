![image](https://cdn.qwertycoin.org/images/press/other/qwc-github.png)

[![Build Status](https://travis-ci.org/qwertycoin-org/qwertycoin.svg?branch=stage_1)](https://travis-ci.org/qwertycoin-org/qwertycoin)
[![Build status](https://ci.appveyor.com/api/projects/status/yhiqfap4nfdommsb?svg=true)](https://ci.appveyor.com/project/qwertycoin-org/qwertycoin)

**HARDFORK 5 IS SET AT HEIGHT 250720!!**
Pools operating **'cryptonote-forknote-pool'** or compatible software should update Node-Cryptonote-Util to this version: https://github.com/qwertycoin-org/node-cryptonote-util. The reference pool (qwertycoin.site) software is here: https://github.com/qwertycoin-org/qwertycoin-pool.
Pools operating **'cryptonote-nodejs-pool'** should _change config on hardfork height_. The changes in config are:
```
"daemonType": "default",
"cnAlgorithm": "cryptonight",
"cnVariant": 0,
"cnBlobType": 0,
```
The example of Qwertycoin config is here: https://github.com/dvandal/cryptonote-nodejs-pool/blob/master/config_examples/qwertycoin.json

### How To Compile

#### Ubuntu 16.04+ and MacOS 10.10+

There is a bash installation script for Ubuntu 16.04+ and MacOS 10.10+ which can be used to checkout and build the project from source:

`$ curl -sL "https://cdn.qwertycoin.org/cmd/multi_installer.sh" | bash `

On Ubuntu you will be asked for sudo rights to install software. The binaries will be in `./src` after compilation is complete.

This script can be used from inside the git repository to build the project from the checked out source, `./multi_installer.sh`

See the script for more installation details and please consider extending it for your operating system and distribution!

If the script doesn't work for you: use `./easy-installer.sh` or with:

#### Linux

If you are using Arch Linux, there is an AUR precompiled package, `qwertycoin-bin`, or a build from source version, `qwertycoin-git`.

##### Prerequisites

- You will need the following packages: build-essential, cmake (3.10 or higher) and git
- Most of these should already be installed on your system. For example on Ubuntu by running:
```
sudo apt-get install build-essential cmake git
```
- After installing dependencies run simple script:
```
git clone https://github.com/qwertycoin-org/qwertycoin
cd ./qwertycoin
mkdir ./build
cd ./build
cmake ..
cmake --build . -config Release
```
- If all went well, it will complete successfully, and you will find all your binaries in the `./build/src` directory.

#### Windows 10

##### Prerequisites

- Install [Visual Studio 2017 Community Edition](https://www.visualstudio.com/thank-you-downloading-visual-studio/?sku=Community&rel=15&page=inlineinstall)
- When installing Visual Studio, it is **required** that you install **Desktop development with C++** and the **VC++ v140 toolchain** when selecting features. The option to install the v140 toolchain can be found by expanding the "Desktop development with C++" node on the right. You will need this for the project to build correctly.
- Make sure that bundled cmake version is 3.10 or higher

##### Building

- From the start menu, open "x64 Native Tools Command Prompt for vs2017"
- And the run the following commands:
```
git clone https://github.com/qwertycoin-org/qwertycoin
cd qwertycoin
md build
cd build
cmake -G "Visual Studio 15 2017 Win64" ..
cmake --build . --config Release
```
- If all went well, it will complete successfully, and you will find all your binaries in the `.\build\src\Release` directory.
- Additionally, a `.sln` file will have been created in the `build` directory. If you wish to open the project in Visual Studio with this, you can.

#### Apple

##### Prerequisites

- Install Xcode and Developer Tools.
- Install [cmake](https://cmake.org/). See [here](https://stackoverflow.com/questions/23849962/cmake-installer-for-mac-fails-to-create-usr-bin-symlinks) if you are unable to call `cmake` from the terminal after installing.
- Install git

##### Building

- After installing dependencies run simple script:
```
git clone https://github.com/qwertycoin-org/qwertycoin
cd ./qwertycoin
mkdir ./build
cd ./build
cmake ..
cmake --build . --config Release
```
- If all went well, it will complete successfully, and you will find all your binaries in the `./build/src` directory.

**Advanced options:**

* Parallel build: run `make -j<number of threads>` instead of `make`.
* Debug build: run `make build-debug`.
* Test suite: run `make test-release` to run tests in addition to building. Running `make test-debug` will do the same to the debug version.
* Building with Clang: it may be possible to use Clang instead of GCC, but this may not work everywhere. To build, run `export CC=clang CXX=clang++` before running `make`.


### Building for Android on Linux

Set up the 32 bit toolchain
Download and extract the Android SDK and NDK
```
android-ndk-r15c/build/tools/make_standalone_toolchain.py --api 21 --stl=libc++ --arch arm --install-dir /opt/android/tool32
```

Download and setup the Boost 1.65.1 source
```
wget https://sourceforge.net/projects/boost/files/boost/1.65.1/boost_1_65_1.tar.bz2/download -O boost_1_65_1.tar.bz2
tar xjf boost_1_65_1.tar.bz2
cd boost_1_65_1
./bootstrap.sh
```
apply patch from external/boost1_65_1/libs/filesystem/src

Build Boost with the 32 bit toolchain
```
export PATH=/opt/android/tool32/arm-linux-androideabi/bin:/opt/android/tool32/bin:$PATH
./b2 abi=aapcs architecture=arm binary-format=elf address-model=32 link=static runtime-link=static --with-chrono --with-date_time --with-filesystem --with-program_options --with-regex --with-serialization --with-system --with-thread --with-context --with-coroutine --with-atomic --build-dir=android32 --stagedir=android32 toolset=clang threading=multi threadapi=pthread target-os=android --reconfigure stage
```

Build Qwertycoin for 32 bit Android
```
mkdir -p build/release.android32
cd build/release.android32
CC=clang CXX=clang++ cmake -D BUILD_TESTS=OFF -D ARCH="armv7-a" -ldl -D STATIC=ON -D BUILD_64=OFF -D CMAKE_BUILD_TYPE=release -D ANDROID=true -D BUILD_TAG="android" -D BOOST_ROOT=/opt/android/boost_1_65_1 -D BOOST_LIBRARYDIR=/opt/android/boost_1_65_1/android32/lib -D CMAKE_POSITION_INDEPENDENT_CODE:BOOL=true -D BOOST_IGNORE_SYSTEM_PATHS_DEFAULT=ON ../..
make SimpleWallet
```

## Donate
Qwertycoin:
```
QWC1K6XEhCC1WsZzT9RRVpc1MLXXdHVKt2BUGSrsmkkXAvqh52sVnNc1pYmoF2TEXsAvZnyPaZu8MW3S8EWHNfAh7X2xa63P7Y
```
```
BTC: 1DkocMNiqFkbjhCmG4sg9zYQbi4YuguFWw
```
```
ETH: 0xA660Fb28C06542258bd740973c17F2632dff2517
```
```
BCH: qz975ndvcechzywtz59xpkt2hhdzkzt3vvt8762yk9
```
```
XMR: 47gmN4GMQ17Veur5YEpru7eCQc5A65DaWUThZa9z9bP6jNMYXPKAyjDcAW4RzNYbRChEwnKu1H3qt9FPW9CnpwZgNscKawX
```
```
ETN: etnkJXJFqiH9FCt6Gq2HWHPeY92YFsmvKX7qaysvnV11M796Xmovo2nSu6EUCMnniqRqAhKX9AQp31GbG3M2DiVM3qRDSQ5Vwq
```

#### Thanks
Cryptonote Developers, Bytecoin Developers, Monero Developers, Karbo Project (aivve), Qwertycoin Community
