#!/bin/bash
set -e
cd utils/lua_tests/
./../../anura --tests --utility=lua -e"_U=true" all.lua
cd ../..
