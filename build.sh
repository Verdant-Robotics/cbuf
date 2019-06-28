#!/bin/bash

mkdir -p build && cd build && cmake -DCBUF_ENABLE_TESTS=ON -DCMAKE_BUILD_TYPE=Debug .. && make -j

#mkdir -p build && cd build && cmake .. && make -j