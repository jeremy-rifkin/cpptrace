#!/bin/bash

echo "Removing .c and .h files"
rm -rfv libdwarf/*.c libdwarf/*.h libdwarf/cmake/config.h.cmake
echo "Fetching current state"
git clone https://github.com/davea42/libdwarf-code.git
echo "Copying files"
mv -v libdwarf-code/src/lib/libdwarf/*.c libdwarf
mv -v libdwarf-code/src/lib/libdwarf/*.h libdwarf
mv -v libdwarf-code/src/lib/libdwarf/LGPL.txt libdwarf
mv -v libdwarf-code/cmake/config.h.cmake libdwarf/cmake
echo "Deleting cloned repo"
rm -rf libdwarf-code
