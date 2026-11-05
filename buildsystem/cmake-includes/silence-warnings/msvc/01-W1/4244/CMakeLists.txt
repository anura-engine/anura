# C4244: conversion to a smaller type may lose data.
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/editor.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4244
)
