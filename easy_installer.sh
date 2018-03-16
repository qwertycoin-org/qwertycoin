#!/usr/bin/env bash
# Qwertycoin developers 2018
# use this installer to clone-and-compile qwertycoin in one line
# supports Ubuntu 16 LTS

sudo apt-get update
yes "" | sudo apt-get install build-essential python-dev gcc-4.9 g++-4.9 git cmake libboost-all-dev
export CXXFLAGS="-std=gnu++11"
git clone https://github.com/qwertycoin-org/qwertycoin
cd qwertycoin
mkdir build && cd $_
cmake ..
make
