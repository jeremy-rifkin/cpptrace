#!/bin/bash
sudo apt install libgtest-dev

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
git remote add origin https://github.com/jeremy-rifkin/libdwarf-lite.git
git fetch --depth 1 origin 5c0cb251f94b27e90184e6b2d9a0c9c62593babc
git checkout FETCH_HEAD
mkdir build
cd build
cmake .. -DPIC_ALWAYS=TRUE -DBUILD_DWARFDUMP=FALSE
make -j
sudo make install
