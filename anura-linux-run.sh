#!/bin/bash
#This is the Steam launch script for Anura, with a few parameters tweaked for Linux.

LD_LIBRARY_PATH=./sos/:$LD_LIBRARY_PATH ./anura "$@"
