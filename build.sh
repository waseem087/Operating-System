#!/bin/bash

# Check if Boost libraries are installed
if ! dpkg -l | grep -q libboost-all-dev; then
    echo "Error: Boost libraries not found."
    echo "Please install them using: sudo apt-get install libboost-all-dev"
    exit 1
fi

echo "Compiling main.cpp..."

# Compile with Boost libraries and threading support
g++ -std=c++17 -pthread main.cpp -o atcs_simulation \
    -lboost_system -lboost_thread

# Check if compilation succeeded
if [ $? -eq 0 ]; then
    echo "Compilation successful!"
    echo "To run the simulation, use: ./atcs_simulation"
else
    echo "Compilation failed. Please check the error messages above."
    exit 1
fi