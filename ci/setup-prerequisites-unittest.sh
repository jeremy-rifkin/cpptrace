#!/bin/bash

mkdir zstd
cd zstd
git init
git remote add origin https://github.com/facebook/zstd.git
git fetch --depth 1 origin 63779c798237346c2b245c546c40b72a5a5913fe # 1.5.5
git checkout FETCH_HEAD
make -j
sudo make install

cd ..

mkdir libdwarf
cd libdwarf
git init
git remote add origin https://github.com/davea42/libdwarf-code.git
git fetch --depth 1 origin e2ab28a547ed8a53f2c96a825247a7cc8f7e40bb
git checkout FETCH_HEAD
mkdir build
cd build
cmake .. -DPIC_ALWAYS=TRUE -DBUILD_DWARFDUMP=FALSE
make -j
sudo make install

cd ../..

mkdir googletest
cd googletest
git init
git remote add origin https://github.com/google/googletest.git
git fetch --depth 1 origin f8d7d77c06936315286eb55f8de22cd23c188571
git checkout FETCH_HEAD
mkdir build
cd build
cmake ..
make -j
sudo make install
