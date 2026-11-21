# C4267: conversion from size_t to a smaller type may lose data.
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/module.cpp"
    "${ANURA_SOURCE_DIR}/src/stats.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4267
)
