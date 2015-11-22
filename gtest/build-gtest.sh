#!/bin/sh

if [ -d gtest ]; then
    echo gtest directory already exists! Continuing will wipe it.
    read -p "Continue? (y/n)" CONT
    if [ $CONT != "y" ]; then
        echo "Aborting..."
        exit 1
    fi
    
    rm -rf gtest
fi

git clone https://github.com/google/googletest.git gtest
cd gtest
git checkout tags/release-1.7.0

mkdir -p build && cd build
cmake -G"Unix Makefiles" ..
make


