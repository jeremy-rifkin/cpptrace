#!/bin/bash
sudo apt install libgtest-dev 2>/dev/null || true

if [ ! -f zstd/lib/libzstd.a ]; then
    mkdir zstd
    cd zstd
    git init
    git remote add origin https://github.com/facebook/zstd.git
    git fetch --depth 1 origin 794ea1b0afca0f020f4e57b6732332231fb23c70 # 1.5.6
    git checkout FETCH_HEAD
    make -j
    cd ..
fi
cd zstd && sudo make install && cd ..

if [ ! -f libdwarf/build/src/lib/libdwarf/libdwarf.a ]; then
    mkdir libdwarf
    cd libdwarf
    git init
    git remote add origin https://github.com/jeremy-rifkin/libdwarf-lite.git
    git fetch --depth 1 origin d06f37ed6660b324e458387d43efde3d84c9dd55 # 2.3.1
    git checkout FETCH_HEAD
    mkdir build
    cd build
    cmake .. -DPIC_ALWAYS=TRUE -DBUILD_DWARFDUMP=FALSE
    make -j
    cd ../..
fi
cd libdwarf/build && sudo make install && cd ../..
