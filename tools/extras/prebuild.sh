#!/bin/bash

BUILD_PATH="$1"
BUILD_SOURCE_PATH="$2"

# Create sketch dir if not exists
if [ ! -f "$BUILD_PATH/sketch" ]; then
  mkdir -p "$BUILD_PATH/sketch"
fi

# Create empty build.opt.h if not exists in the original sketch dir
if [ ! -f "$BUILD_SOURCE_PATH/build_opt.h" ]; then
  touch "$BUILD_PATH/sketch/build_opt.h"
fi
