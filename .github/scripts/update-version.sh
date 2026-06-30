#!/bin/bash
# Disable shellcheck warning about using 'cat' to read a file.
# shellcheck disable=SC2002

# Exit on any error and make pipelines fail if any command fails
set -e
set -o pipefail

# For reference: add tools for all boards by replacing one line in each board
# "[board].upload.tool=esptool_py" to "[board].upload.tool=esptool_py\n[board].upload.tool.default=esptool_py\n[board].upload.tool.network=esp_ota"
#cat boards.txt | sed "s/\([a-zA-Z0-9_\-]*\)\.upload\.tool\=esptool_py/\1\.upload\.tool\=esptool_py\\n\1\.upload\.tool\.default\=esptool_py\\n\1\.upload\.tool\.network\=esp_ota/"

if [ ! $# -eq 1 ]; then
    echo "Bad number of arguments: $#" >&2
    echo "usage: $0 <version>" >&2
    exit 1
fi

# Version must be in the format of X.Y.Z or X.Y.Z-abc123 (POSIX ERE)
re='^[0-9]+\.[0-9]+\.[0-9]+(-[A-Za-z]+[0-9]*)?$'
version=$1

if [[ ! $version =~ $re ]] ; then
    echo "error: Not a valid version: $version" >&2
    echo "usage: $0 <version>" >&2
    exit 1
fi

ESP_ARDUINO_VERSION_MAJOR=$(echo "$version" | cut -d. -f1)
ESP_ARDUINO_VERSION_MINOR=$(echo "$version" | cut -d. -f2)
ESP_ARDUINO_VERSION_PATCH=$(echo "$version" | cut -d. -f3 | sed 's/[^0-9].*//')  # Remove non-numeric suffixes like RC1, alpha, beta
ESP_ARDUINO_VERSION_CLEAN="$ESP_ARDUINO_VERSION_MAJOR.$ESP_ARDUINO_VERSION_MINOR.$ESP_ARDUINO_VERSION_PATCH"

# Get ESP-IDF version from build_component.yml (this way we can ensure that the version is correct even if the local libs are not up to date)
ESP_IDF_VERSION=$(grep -m 1 "default:" .github/workflows/build_component.yml | sed 's/.*release-v\([^"]*\).*/\1/')
if [ -z "$ESP_IDF_VERSION" ]; then
    echo "Error: ESP-IDF version not found in build_component.yml" >&2
    exit 1
fi

echo "New Arduino Version: $version"
echo "ESP-IDF Version: $ESP_IDF_VERSION"

echo "Updating issue template..."
if ! grep -q "^[[:space:]]*- v$version$" .github/ISSUE_TEMPLATE/Issue-report.yml; then
    cat .github/ISSUE_TEMPLATE/Issue-report.yml | \
    sed "s/.*\- latest master .*/        - latest master \(checkout manually\)\\n        - v$version/g" > __issue-report.yml && mv __issue-report.yml .github/ISSUE_TEMPLATE/Issue-report.yml
    echo "Issue template updated with version v$version"
else
    echo "Version v$version already exists in issue template, skipping update"
fi

echo "Updating GitLab variables..."
cat .gitlab/workflows/common.yml | \
sed "s/ESP_IDF_VERSION:.*/ESP_IDF_VERSION: \"$ESP_IDF_VERSION\"/g" | \
sed "s/ESP_ARDUINO_VERSION:.*/ESP_ARDUINO_VERSION: \"$ESP_ARDUINO_VERSION_CLEAN\"/g" > .gitlab/workflows/__common.yml && mv .gitlab/workflows/__common.yml .gitlab/workflows/common.yml

echo "Updating platform.txt..."
cat platform.txt | sed "s/version=.*/version=$ESP_ARDUINO_VERSION_CLEAN/g" > __platform.txt && mv __platform.txt platform.txt

echo "Updating package.json..."
cat package.json | sed "s/.*\"version\":.*/  \"version\": \"$ESP_ARDUINO_VERSION_CLEAN\",/g" > __package.json && mv __package.json package.json

echo "Updating docs/conf_common.py..."
cat docs/conf_common.py | \
sed "s/.. |version| replace:: .*/.. |version| replace:: $ESP_ARDUINO_VERSION_CLEAN/g" | \
sed "s/.. |idf_version| replace:: .*/.. |idf_version| replace:: $ESP_IDF_VERSION/g" > docs/__conf_common.py && mv docs/__conf_common.py docs/conf_common.py

echo "Updating .gitlab/workflows/common.yml..."
cat .gitlab/workflows/common.yml | \
sed "s/ESP_IDF_VERSION:.*/ESP_IDF_VERSION: \"$ESP_IDF_VERSION\"/g" | \
sed "s/ESP_ARDUINO_VERSION:.*/ESP_ARDUINO_VERSION: \"$ESP_ARDUINO_VERSION_CLEAN\"/g" > .gitlab/workflows/__common.yml && mv .gitlab/workflows/__common.yml .gitlab/workflows/common.yml

echo "Updating cores/esp32/esp_arduino_version.h..."
cat cores/esp32/esp_arduino_version.h | \
sed "s/#define ESP_ARDUINO_VERSION_MAJOR.*/#define ESP_ARDUINO_VERSION_MAJOR $ESP_ARDUINO_VERSION_MAJOR/g" | \
sed "s/#define ESP_ARDUINO_VERSION_MINOR.*/#define ESP_ARDUINO_VERSION_MINOR $ESP_ARDUINO_VERSION_MINOR/g" | \
sed "s/#define ESP_ARDUINO_VERSION_PATCH.*/#define ESP_ARDUINO_VERSION_PATCH $ESP_ARDUINO_VERSION_PATCH/g" > __esp_arduino_version.h && mv __esp_arduino_version.h cores/esp32/esp_arduino_version.h

libraries=$(find libraries -maxdepth 1 -mindepth 1 -type d -exec basename {} \;)
for lib in $libraries; do
    if [ -f "libraries/$lib/library.properties" ]; then
        echo "Updating Library $lib..."
        cat "libraries/$lib/library.properties" | sed "s/version=.*/version=$ESP_ARDUINO_VERSION_CLEAN/g" > "libraries/$lib/__library.properties" && mv "libraries/$lib/__library.properties" "libraries/$lib/library.properties"
    fi
done

exit 0
