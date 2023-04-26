#!/bin/bash
HELP="This script help to add library when using arduino-esp32 as an ESP-IDF component
The script accepts up to three arguments:
-n  NEW: URL address to new library on GIThub (cannot be combined with -e)
-l  LOCAL: Path to the project where the library should be placed locally
-e  EXISTING: path to existing libary- this will simply skip the download (cannot be combined with -n)

Examples:
./add_lib.sh -n https://github.com/me-no-dev/ESPAsyncWebServer
./add_lib.sh -l ~/esp/esp-idf/examples/your_project
./add_lib.sh -e ~/Arduino/libraries/existing_library

./add_lib.sh -n https://github.com/me-no-dev/ESPAsyncWebServer -l ~/esp/esp-idf/examples/your_project
./add_lib.sh -e ~/Arduino/libraries/existing_library -l ~/esp/esp-idf/examples/your_project"

# Get the directory name where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# Construct the absolute path to libraries folder
ARDUINO_LIBS_PATH="$SCRIPT_DIR/../libraries"

# Define the default values for the parameters
e_param=""
l_param=""
n_param=""

# Parse the command-line arguments using getopts
while getopts "he:l:n:" opt; do
  case $opt in
    h)
      echo "$HELP"
      exit 0
      ;;
    e)
      #e_param="$OPTARG"
      e_param="${OPTARG/#~/$HOME}"
      ;;
    l)
      #l_param="$OPTARG"
      l_param="${OPTARG/#~/$HOME}"
      ;;
    n)
      n_param=$OPTARG
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      echo $HELP
      exit 1
      ;;
    :)
      echo "Option -$OPTARG requires an argument." >&2
      echo $HELP
      exit 1
      ;;
  esac
done

# Debug Print the parameters
echo "e_param: $e_param"
echo "l_param: $l_param"
echo "n_param: $n_param"

# No parameter check
if [[ -z "$e_param" ]] && [[ -z "$e_param" ]] && [[ -z "$e_param" ]]; then
  echo "Error: No parameters" >&2
  echo "$HELP"
  exit 1

# Invalid combination check
if [[ ! -z $e_param ]] && [[ ! -z $n_param ]]; then
  echo "ERROR: Cannot combine -n with -e" >&2
  echo "$HELP"
  exit 1
fi
fi

# Check existing lib
if [[ ! -z "$e_param" ]]; then
  echo "DEBUG: existing lib parameter supplied"
  if  [[ ! -d "${e_param/#~/$HOME}" ]]; then # this works!
    echo "Error: existing library parameter - path does not exist" >&2
    exit 1
  fi
fi

exit # Script is not yet finished and tested - use at your own risk!

LIBRARY=""

# Install new lib
if [ ! -z $n_param ]; then
  INSTALL_TARGET=""
  if [ -z $l_param ]; then
    # If local path for project is not supplied - use as INSTALL_TARGET Arduino libraries path
    INSTALL_TARGET=$($ARDUINO_LIBS_PATH/$(basename "$n_param"))
  else
    INSTALL_TARGET=$l_param/components/$(basename "$n_param")
  fi

  # clone the new lib
  echo "Cloning: git clone --recursive $n_param $INSTALL_TARGET"
  git clone --recursive $n_param $INSTALL_TARGET
  LIBRARY=$INSTALL_TARGET
fi

# Copy existing lib to local project
if [[ ! -z $e_param ]] && [[ ! -z $l_param ]]; then
  echo "Copy from $e_param to $l_param"
  echo "cp -r $e_param $l_param/components/$(basename "$e_param")"
  cp -r $e_param $l_param/components/$(basename "$e_param")
  LIBRARY=$l_param/components/$(basename "$e_param")
fi

# If Local target was supplied use that path, otherwise use Arduino path
#if [ ! -z $l_param ]; then
#  echo "DEBUG: Local lib path"
#  if [ ! -z $e_param ]; then
#    echo "DEBUG: from existing"
#    LIBRARY=$l_param/components/$(basename "$e_param")
#  elif [ ! -z $n_param ]; then
#   echo "DEBUG: from new"
    #LIBRARY=$INSTALL_TARGET
#  fi
#else
#  echo "DEBUG: Arduino lib path"
#  LIBRARY=$($ARDUINO_LIBS_PATH/$(basename "$n_param"))
#fi

echo "DEBUG: LIBRARY: $LIBRARY"


if [ -z LIBRARY ]; then
  echo "ERROR: No library -e" >&2
  exit 1
fi

# Generate CMakeLists.txt

# 1. get the source list:
FILES=$(find $LIBRARY -name '*.c' -o -name '*.cpp' |  xargs -I{} basename {})
echo "DEBUG: source files:"
echo "$FILES"

rm $LIBRARY/CMakeLists.txt # Fresh start
echo "idf_component_register(SRCS $(echo $FILES | sed -e 's/ /" "/g' | sed -e 's/^/"/' -e 's/$/"/')" >> $LIBRARY/CMakeLists.txt
echo "                       INCLUDE_DIRS \".\"" >> $LIBRARY/CMakeLists.txt
echo "                       REQUIRES \"arduino-esp32\"" >> $LIBRARY/CMakeLists.txt
echo "                       )" >> $LIBRARY/CMakeLists.txt
