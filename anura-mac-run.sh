#!/bin/bash
#This is the Steam launch script for Anura, with a few parameters tweaked for mac

DYLD_LIBRARY_PATH=./Contents/Frameworks ./Contents/MacOS/Frogatto "$@"
