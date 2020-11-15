#!/bin/bash
#This is the Steam launch script for Anura.

LD_LIBRARY_PATH=./runtime:$LD_LIBRARY_PATH ./anura "$@"
