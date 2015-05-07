#!/bin/bash
set -e
if [ -f "test.tmp" ]
then
  rm test.tmp
fi
./anura --tests --utility=test_persist utils/eris_tests/persist.lua test.tmp
./anura --tests --utility=test_unpersist utils/eris_tests/unpersist.lua test.tmp
