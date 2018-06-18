#!/bin/bash
#
# This script is for Travis. It checks all non-examples source files in libraries/ and cores/ are listed in
# CMakeLists.txt for the cmake-based IDF component
#
# If you see an error running this script, edit CMakeLists.txt and add any new source files into your PR
#

set -e

cd "`dirname $0`/.."  # cd to arduino-esp32 root

# find all source files in repo
REPO_SRCS=`find cores/esp32/ libraries/ -name 'examples' -prune -o -name 'main.cpp' -prune -o -name '*.c' -print -o -name '*.cpp' -print | sort`

# find all source files named in CMakeLists.txt COMPONENT_SRCS
CMAKE_SRCS=`cmake --trace-expand -C CMakeLists.txt 2>&1 | grep COMPONENT_SRCS | sed 's/.\+COMPONENT_SRCS //' | sed 's/ )//' | tr ' ;' '\n' | sort`

if ! diff -u0 --label "Repo Files" --label "COMPONENT_SRCS" <(echo "$REPO_SRCS") <(echo "$CMAKE_SRCS"); then
    echo "Source files in repo (-) and source files in CMakeLists.txt (+) don't match"
    echo "Edit CMakeLists.txt as appropriate to add/remove source files from COMPONENT_SRCS"
    exit 1
fi

echo "CMakeLists.txt and repo source files match"
exit 0
