#!/bin/bash
# Disable shellcheck warning about using 'cat' to read a file.
# shellcheck disable=SC2002

# For reference: add tools for all boards by replacing one line in each board
# "[board].upload.tool=esptool_py" to "[board].upload.tool=esptool_py\n[board].upload.tool.default=esptool_py\n[board].upload.tool.network=esp_ota"
#cat boards.txt | sed "s/\([a-zA-Z0-9_\-]*\)\.upload\.tool\=esptool_py/\1\.upload\.tool\=esptool_py\\n\1\.upload\.tool\.default\=esptool_py\\n\1\.upload\.tool\.network\=esp_ota/"

if [ ! $# -eq 3 ]; then
    echo "Bad number of arguments: $#" >&2
    echo "usage: $0 <major> <minor> <patch>" >&2
    exit 1
fi

re='^[0-9]+$'
if [[ ! $1 =~ $re ]] || [[ ! $2 =~ $re ]] || [[ ! $3 =~ $re ]] ; then
    echo "error: Not a valid version: $1.$2.$3" >&2
    echo "usage: $0 <major> <minor> <patch>" >&2
    exit 1
fi

ESP_ARDUINO_VERSION_MAJOR="$1"
ESP_ARDUINO_VERSION_MINOR="$2"
ESP_ARDUINO_VERSION_PATCH="$3"
ESP_ARDUINO_VERSION="$ESP_ARDUINO_VERSION_MAJOR.$ESP_ARDUINO_VERSION_MINOR.$ESP_ARDUINO_VERSION_PATCH"

# Get ESP-IDF version from push.yml (this way we can ensure that the version is correct even if the local libs are not up to date)
ESP_IDF_VERSION=$(grep "idf_ver:" .github/workflows/push.yml | sed 's/.*release-v\([^"]*\).*/\1/')
if [ -z "$ESP_IDF_VERSION" ]; then
    echo "Error: ESP-IDF version not found in push.yml" >&2
    exit 1
fi

echo "New Arduino Version: $ESP_ARDUINO_VERSION"
echo "ESP-IDF Version: $ESP_IDF_VERSION"

echo "Updating platform.txt..."
cat platform.txt | sed "s/version=.*/version=$ESP_ARDUINO_VERSION/g" > __platform.txt && mv __platform.txt platform.txt

echo "Updating package.json..."
cat package.json | sed "s/.*\"version\":.*/  \"version\": \"$ESP_ARDUINO_VERSION\",/g" > __package.json && mv __package.json package.json

echo "Updating docs/conf_common.py..."
cat docs/conf_common.py | \
sed "s/.. |version| replace:: .*/.. |version| replace:: $ESP_ARDUINO_VERSION/g" | \
sed "s/.. |idf_version| replace:: .*/.. |idf_version| replace:: $ESP_IDF_VERSION/g" > docs/__conf_common.py && mv docs/__conf_common.py docs/conf_common.py

echo "Updating .gitlab/workflows/common.yml..."
cat .gitlab/workflows/common.yml | \
sed "s/ESP_IDF_VERSION:.*/ESP_IDF_VERSION: \"$ESP_IDF_VERSION\"/g" | \
sed "s/ESP_ARDUINO_VERSION:.*/ESP_ARDUINO_VERSION: \"$ESP_ARDUINO_VERSION\"/g" > .gitlab/workflows/__common.yml && mv .gitlab/workflows/__common.yml .gitlab/workflows/common.yml

echo "Updating cores/esp32/esp_arduino_version.h..."
cat cores/esp32/esp_arduino_version.h | \
sed "s/#define ESP_ARDUINO_VERSION_MAJOR.*/#define ESP_ARDUINO_VERSION_MAJOR $ESP_ARDUINO_VERSION_MAJOR/g" | \
sed "s/#define ESP_ARDUINO_VERSION_MINOR.*/#define ESP_ARDUINO_VERSION_MINOR $ESP_ARDUINO_VERSION_MINOR/g" | \
sed "s/#define ESP_ARDUINO_VERSION_PATCH.*/#define ESP_ARDUINO_VERSION_PATCH $ESP_ARDUINO_VERSION_PATCH/g" > __esp_arduino_version.h && mv __esp_arduino_version.h cores/esp32/esp_arduino_version.h

libraries=$(find libraries -maxdepth 1 -mindepth 1 -type d -exec basename {} \;)
for lib in $libraries; do
    if [ -f "libraries/$lib/library.properties" ]; then
        echo "Updating Library $lib..."
        cat "libraries/$lib/library.properties" | sed "s/version=.*/version=$ESP_ARDUINO_VERSION/g" > "libraries/$lib/__library.properties" && mv "libraries/$lib/__library.properties" "libraries/$lib/library.properties"
    fi
done

exit 0
