#!/bin/bash
rm -rf libraries
mkdir libraries
pushd libraries
git clone https://github.com/ContinuumIO/libdynd.git
popd
