#!/bin/bash
LIBRARY=""

#TODO project library: -p path_to/project


# this is for new lib: [-n <git-link>]
# Get the directory name where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# Construct the absolute path to libraries folder
LIBS_PATH="$SCRIPT_DIR/../libraries"

# Use $LIBS_PATH variable to clone new lib, regardless of where the script is run from.
echo "Cloning: git clone --recursive $1 $LIBS_PATH/Firebase-ESP32"
git clone --recursive $1 $LIBS_PATH/Firebase-ESP32

LIBRARY=$LIBS_PATH/Firebase-ESP32




# get the CMakelists right









# local:
#Modify your main\CMakeLists.txt to this:
#
#idf_component_register(
#   SRC_DIRS               "."
#                          "libraries/Firebase-ESP32/src"
#                          "libraries/Firebase-ESP32/src/addons"
#                          "libraries/Firebase-ESP32/src/json"
#                          "libraries/Firebase-ESP32/src/json/extras/print"
#                          "libraries/Firebase-ESP32/src/json/MB_JSON"
#                          "libraries/Firebase-ESP32/src/mbfs"
#                          "libraries/Firebase-ESP32/src/rtdb"
#                          "libraries/Firebase-ESP32/src/rtdb/stream"
#                          "libraries/Firebase-ESP32/src/session"
#                          "libraries/Firebase-ESP32/src/signer"
#                          "libraries/Firebase-ESP32/src/sslclient/esp32"
#                          "libraries/Firebase-ESP32/src/wcs"
#                          "libraries/Firebase-ESP32/src/wcs/base"
#                          "libraries/Firebase-ESP32/src/wcs/custom"
#                          "libraries/Firebase-ESP32/src/wcs/esp32"
#
   #
#   INCLUDE_DIRS           "."
#                          "libraries/Firebase-ESP32/src"
#                          "libraries/Firebase-ESP32/src/addons"
#                          "libraries/Firebase-ESP32/src/json"
#                          "libraries/Firebase-ESP32/src/json/extras/print"
#                          "libraries/Firebase-ESP32/src/json/MB_JSON"
#                          "libraries/Firebase-ESP32/src/mbfs"
#                          "libraries/Firebase-ESP32/src/rtdb"
#                          "libraries/Firebase-ESP32/src/rtdb/stream"
#                          "libraries/Firebase-ESP32/src/session"
#                          "libraries/Firebase-ESP32/src/signer"
#                          "libraries/Firebase-ESP32/src/sslclient/esp32"
#                          "libraries/Firebase-ESP32/src/wcs"
#                          "libraries/Firebase-ESP32/src/wcs/base"
#                          "libraries/Firebase-ESP32/src/wcs/custom"
#                          "libraries/Firebase-ESP32/src/wcs/esp32"
#)

