#!/bin/bash
#
# This script is used in the CI workflow. It checks all non-examples source files in libraries/ and cores/ are listed in
# CMakeLists.txt for the cmake-based IDF component
#
# If you see an error running this script, edit CMakeLists.txt and add any new source files into your PR
#

set -e

# pull all submodules
git submodule update --init --recursive

# find all source files in repo
REPO_SRCS=`find cores/esp32/ libraries/ -name 'examples' -prune -o -name '*.c' -print -o -name '*.cpp' -print | sort`

# find all source files named in CMakeLists.txt COMPONENT_SRCS
CMAKE_SRCS=`cmake --trace-expand -P CMakeLists.txt 2>&1 | grep set\(srcs | cut -d'(' -f3 | sed 's/ )//' | sed 's/srcs //' | tr ' ;' '\n' | sort`

if ! diff -u0 --label "Repo Files" --label "srcs" <(echo "$REPO_SRCS") <(echo "$CMAKE_SRCS"); then
    echo "Source files in repo (-) and source files in CMakeLists.txt (+) don't match"
    echo "Edit CMakeLists.txt as appropriate to add/remove source files from COMPONENT_SRCS"
    exit 1
fi

echo "CMakeLists.txt and repo source files match"
exit 0
