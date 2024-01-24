#!/bin/bash

if [ ! $GITHUB_EVENT_NAME == "release" ]; then
    echo "Wrong event '$GITHUB_EVENT_NAME'!"
    exit 1
fi

EVENT_JSON=`cat $GITHUB_EVENT_PATH`

action=`echo $EVENT_JSON | jq -r '.action'`
if [ ! $action == "published" ]; then
    echo "Wrong action '$action'. Exiting now..."
    exit 0
fi

draft=`echo $EVENT_JSON | jq -r '.release.draft'`
if [ $draft == "true" ]; then
    echo "It's a draft release. Exiting now..."
    exit 0
fi

RELEASE_PRE=`echo $EVENT_JSON | jq -r '.release.prerelease'`
RELEASE_TAG=`echo $EVENT_JSON | jq -r '.release.tag_name'`
RELEASE_BRANCH=`echo $EVENT_JSON | jq -r '.release.target_commitish'`
RELEASE_ID=`echo $EVENT_JSON | jq -r '.release.id'`

OUTPUT_DIR="$GITHUB_WORKSPACE/build"
PACKAGE_NAME="esp32-$RELEASE_TAG"
PACKAGE_JSON_MERGE="$GITHUB_WORKSPACE/.github/scripts/merge_packages.py"
PACKAGE_JSON_TEMPLATE="$GITHUB_WORKSPACE/package/package_esp32_index.template.json"
PACKAGE_JSON_DEV="package_esp32_dev_index.json"
PACKAGE_JSON_REL="package_esp32_index.json"

echo "Event: $GITHUB_EVENT_NAME, Repo: $GITHUB_REPOSITORY, Path: $GITHUB_WORKSPACE, Ref: $GITHUB_REF"
echo "Action: $action, Branch: $RELEASE_BRANCH, ID: $RELEASE_ID"
echo "Tag: $RELEASE_TAG, Draft: $draft, Pre-Release: $RELEASE_PRE"

function get_file_size(){
    local file="$1"
    if [[ "$OSTYPE" == "darwin"* ]]; then
        eval `stat -s "$file"`
        local res="$?"
        echo "$st_size"
        return $res
    else
        stat --printf="%s" "$file"
        return $?
    fi
}

function git_upload_asset(){
    local name=$(basename "$1")
    # local mime=$(file -b --mime-type "$1")
    curl -k -X POST -sH "Authorization: token $GITHUB_TOKEN" -H "Content-Type: application/octet-stream" --data-binary @"$1" "https://uploads.github.com/repos/$GITHUB_REPOSITORY/releases/$RELEASE_ID/assets?name=$name"
}

function git_safe_upload_asset(){
    local file="$1"
    local name=$(basename "$file")
    local size=`get_file_size "$file"`
    local upload_res=`git_upload_asset "$file"`
    if [ $? -ne 0 ]; then
        >&2 echo "ERROR: Failed to upload '$name' ($?)"
        return 1
    fi
    up_size=`echo "$upload_res" | jq -r '.size'`
    if [ $up_size -ne $size ]; then
        >&2 echo "ERROR: Uploaded size does not match! $up_size != $size"
        #git_delete_asset
        return 1
    fi
    echo "$upload_res" | jq -r '.browser_download_url'
    return $?
}

function git_upload_to_pages(){
    local path=$1
    local src=$2

    if [ ! -f "$src" ]; then
        >&2 echo "Input is not a file! Aborting..."
        return 1
    fi

    local info=`curl -s -k -H "Authorization: token $GITHUB_TOKEN" -H "Accept: application/vnd.github.v3.object+json" -X GET "https://api.github.com/repos/$GITHUB_REPOSITORY/contents/$path?ref=gh-pages"`
    local type=`echo "$info" | jq -r '.type'`
    local message=$(basename $path)
    local sha=""
    local content=""

    if [ $type == "file" ]; then
        sha=`echo "$info" | jq -r '.sha'`
        sha=",\"sha\":\"$sha\""
        message="Updating $message"
    elif [ ! $type == "null" ]; then
        >&2 echo "Wrong type '$type'"
        return 1
    else
        message="Creating $message"
    fi

    content=`base64 -i "$src"`
    data="{\"branch\":\"gh-pages\",\"message\":\"$message\",\"content\":\"$content\"$sha}"

    echo "$data" | curl -s -k -H "Authorization: token $GITHUB_TOKEN" -H "Accept: application/vnd.github.v3.raw+json" -X PUT --data @- "https://api.github.com/repos/$GITHUB_REPOSITORY/contents/$path"
}

function git_safe_upload_to_pages(){
    local path=$1
    local file="$2"
    local name=$(basename "$file")
    local size=`get_file_size "$file"`
    local upload_res=`git_upload_to_pages "$path" "$file"`
    if [ $? -ne 0 ]; then
        >&2 echo "ERROR: Failed to upload '$name' ($?)"
        return 1
    fi
    up_size=`echo "$upload_res" | jq -r '.content.size'`
    if [ $up_size -ne $size ]; then
        >&2 echo "ERROR: Uploaded size does not match! $up_size != $size"
        #git_delete_asset
        return 1
    fi
    echo "$upload_res" | jq -r '.content.download_url'
    return $?
}

function merge_package_json(){
    local jsonLink=$1
    local jsonOut=$2
    local old_json=$OUTPUT_DIR/oldJson.json
    local merged_json=$OUTPUT_DIR/mergedJson.json

    echo "Downloading previous JSON $jsonLink ..."
    curl -L -o "$old_json" "https://github.com/$GITHUB_REPOSITORY/releases/download/$jsonLink?access_token=$GITHUB_TOKEN" 2>/dev/null
    if [ $? -ne 0 ]; then echo "ERROR: Download Failed! $?"; exit 1; fi

    echo "Creating new JSON ..."
    set +e
    stdbuf -oL python "$PACKAGE_JSON_MERGE" "$jsonOut" "$old_json" > "$merged_json"
    set -e

    set -v
    if [ ! -s $merged_json ]; then
        rm -f "$merged_json"
        echo "Nothing to merge"
    else
        rm -f "$jsonOut"
        mv "$merged_json" "$jsonOut"
        echo "JSON data successfully merged"
    fi
    rm -f "$old_json"
    set +v
}

set -e

##
## PACKAGE ZIP
##

mkdir -p "$OUTPUT_DIR"
PKG_DIR="$OUTPUT_DIR/$PACKAGE_NAME"
PACKAGE_ZIP="$PACKAGE_NAME.zip"

echo "Updating submodules ..."
git -C "$GITHUB_WORKSPACE" submodule update --init --recursive > /dev/null 2>&1

mkdir -p "$PKG_DIR/tools"

# Copy all core files to the package folder
echo "Copying files for packaging ..."
cp -f  "$GITHUB_WORKSPACE/boards.txt"                       "$PKG_DIR/"
cp -f  "$GITHUB_WORKSPACE/package.json"                     "$PKG_DIR/"
cp -f  "$GITHUB_WORKSPACE/programmers.txt"                  "$PKG_DIR/"
cp -Rf "$GITHUB_WORKSPACE/cores"                            "$PKG_DIR/"
cp -Rf "$GITHUB_WORKSPACE/libraries"                        "$PKG_DIR/"
cp -Rf "$GITHUB_WORKSPACE/variants"                         "$PKG_DIR/"
cp -f  "$GITHUB_WORKSPACE/tools/espota.exe"                 "$PKG_DIR/tools/"
cp -f  "$GITHUB_WORKSPACE/tools/espota.py"                  "$PKG_DIR/tools/"
cp -f  "$GITHUB_WORKSPACE/tools/gen_esp32part.py"           "$PKG_DIR/tools/"
cp -f  "$GITHUB_WORKSPACE/tools/gen_esp32part.exe"          "$PKG_DIR/tools/"
cp -f  "$GITHUB_WORKSPACE/tools/gen_insights_package.py"    "$PKG_DIR/tools/"
cp -f  "$GITHUB_WORKSPACE/tools/gen_insights_package.exe"   "$PKG_DIR/tools/"
cp -Rf "$GITHUB_WORKSPACE/tools/partitions"                 "$PKG_DIR/tools/"
cp -Rf "$GITHUB_WORKSPACE/tools/ide-debug"                  "$PKG_DIR/tools/"
cp -f  "$GITHUB_WORKSPACE/tools/platformio-build.py"        "$PKG_DIR/tools/"

# Remove unnecessary files in the package folder
echo "Cleaning up folders ..."
find "$PKG_DIR" -name '*.DS_Store' -exec rm -f {} \;
find "$PKG_DIR" -name '*.git*' -type f -delete

##
## TEMP WORKAROUND FOR RV32 LONG PATH ON WINDOWS
##
RVTC_NAME="riscv32-esp-elf-gcc"
RVTC_NEW_NAME="esp-rv32"

# Replace tools locations in platform.txt
echo "Generating platform.txt..."
cat "$GITHUB_WORKSPACE/platform.txt" | \
sed "s/version=.*/version=$RELEASE_TAG/g" | \
sed 's/tools.esp32-arduino-libs.path={runtime.platform.path}\/tools\/esp32-arduino-libs/tools.esp32-arduino-libs.path=\{runtime.tools.esp32-arduino-libs.path\}/g' | \
sed 's/tools.xtensa-esp32-elf-gcc.path={runtime.platform.path}\/tools\/xtensa-esp32-elf/tools.xtensa-esp32-elf-gcc.path=\{runtime.tools.xtensa-esp32-elf-gcc.path\}/g' | \
sed 's/tools.xtensa-esp32s2-elf-gcc.path={runtime.platform.path}\/tools\/xtensa-esp32s2-elf/tools.xtensa-esp32s2-elf-gcc.path=\{runtime.tools.xtensa-esp32s2-elf-gcc.path\}/g' | \
sed 's/tools.xtensa-esp32s3-elf-gcc.path={runtime.platform.path}\/tools\/xtensa-esp32s3-elf/tools.xtensa-esp32s3-elf-gcc.path=\{runtime.tools.xtensa-esp32s3-elf-gcc.path\}/g' | \
sed 's/tools.xtensa-esp-elf-gdb.path={runtime.platform.path}\/tools\/xtensa-esp-elf-gdb/tools.xtensa-esp-elf-gdb.path=\{runtime.tools.xtensa-esp-elf-gdb.path\}/g' | \
sed "s/tools.riscv32-esp-elf-gcc.path={runtime.platform.path}\\/tools\\/riscv32-esp-elf/tools.riscv32-esp-elf-gcc.path=\\{runtime.tools.$RVTC_NEW_NAME.path\\}/g" | \
sed 's/tools.riscv32-esp-elf-gdb.path={runtime.platform.path}\/tools\/riscv32-esp-elf-gdb/tools.riscv32-esp-elf-gdb.path=\{runtime.tools.riscv32-esp-elf-gdb.path\}/g' | \
sed 's/tools.esptool_py.path={runtime.platform.path}\/tools\/esptool/tools.esptool_py.path=\{runtime.tools.esptool_py.path\}/g' | \
sed 's/debug.server.openocd.path={runtime.platform.path}\/tools\/openocd-esp32\/bin\/openocd/debug.server.openocd.path=\{runtime.tools.openocd-esp32.path\}\/bin\/openocd/g' | \
sed 's/debug.server.openocd.scripts_dir={runtime.platform.path}\/tools\/openocd-esp32\/share\/openocd\/scripts\//debug.server.openocd.scripts_dir=\{runtime.tools.openocd-esp32.path\}\/share\/openocd\/scripts\//g' | \
sed 's/debug.server.openocd.scripts_dir.windows={runtime.platform.path}\\tools\\openocd-esp32\\share\\openocd\\scripts\\/debug.server.openocd.scripts_dir.windows=\{runtime.tools.openocd-esp32.path\}\\share\\openocd\\scripts\\/g' \
 > "$PKG_DIR/platform.txt"

# Add header with version information
echo "Generating core_version.h ..."
ver_define=`echo $RELEASE_TAG | tr "[:lower:].\055" "[:upper:]_"`
ver_hex=`git -C "$GITHUB_WORKSPACE" rev-parse --short=8 HEAD 2>/dev/null`
echo \#define ARDUINO_ESP32_GIT_VER 0x$ver_hex > "$PKG_DIR/cores/esp32/core_version.h"
echo \#define ARDUINO_ESP32_GIT_DESC `git -C "$GITHUB_WORKSPACE" describe --tags 2>/dev/null` >> "$PKG_DIR/cores/esp32/core_version.h"
echo \#define ARDUINO_ESP32_RELEASE_$ver_define >> "$PKG_DIR/cores/esp32/core_version.h"
echo \#define ARDUINO_ESP32_RELEASE \"$ver_define\" >> "$PKG_DIR/cores/esp32/core_version.h"

# Compress package folder
echo "Creating ZIP ..."
pushd "$OUTPUT_DIR" >/dev/null
zip -qr "$PACKAGE_ZIP" "$PACKAGE_NAME"
if [ $? -ne 0 ]; then echo "ERROR: Failed to create $PACKAGE_ZIP ($?)"; exit 1; fi

# Calculate SHA-256
echo "Calculating SHA sum ..."
PACKAGE_PATH="$OUTPUT_DIR/$PACKAGE_ZIP"
PACKAGE_SHA=`shasum -a 256 "$PACKAGE_ZIP" | cut -f 1 -d ' '`
PACKAGE_SIZE=`get_file_size "$PACKAGE_ZIP"`
popd >/dev/null
rm -rf "$PKG_DIR"
echo "'$PACKAGE_ZIP' Created! Size: $PACKAGE_SIZE, SHA-256: $PACKAGE_SHA"
echo

# Upload package to release page
echo "Uploading package to release page ..."
PACKAGE_URL=`git_safe_upload_asset "$PACKAGE_PATH"`
echo "Package Uploaded"
echo "Download URL: $PACKAGE_URL"
echo

##
## LIBS PACKAGE ZIP
##

LIBS_PROJ_NAME="esp32-arduino-libs"
LIBS_PKG_DIR="$OUTPUT_DIR/$LIBS_PROJ_NAME"
LIBS_PACKAGE_ZIP="$LIBS_PROJ_NAME-$RELEASE_TAG.zip"

# Get the libs package URL from the template
LIBS_PACKAGE_SRC_ZIP="$OUTPUT_DIR/src-$LIBS_PROJ_NAME.zip"
LIBS_PACKAGE_SRC_URL=`cat $PACKAGE_JSON_TEMPLATE | jq -r ".packages[0].tools[] | select(.name==\"$LIBS_PROJ_NAME\") | .systems[0].url"`

# Download the libs package
echo "Downloading the libs archive ..."
curl -o "$LIBS_PACKAGE_SRC_ZIP" -LJO --url "$LIBS_PACKAGE_SRC_URL" || exit 1

# Extract the libs package
echo "Extracting the archive ..."
unzip -q -d "$OUTPUT_DIR" "$LIBS_PACKAGE_SRC_ZIP" || exit 1
EXTRACTED_DIR=`ls "$OUTPUT_DIR" | grep "^$LIBS_PROJ_NAME"`
mv "$OUTPUT_DIR/$EXTRACTED_DIR" "$LIBS_PKG_DIR" || exit 1

# Remove unnecessary files in the package folder
echo "Cleaning up folders ..."
find "$LIBS_PKG_DIR" -name '*.DS_Store' -exec rm -f {} \;
find "$LIBS_PKG_DIR" -name '*.git*' -type f -delete

# Compress package folder
echo "Creating ZIP ..."
pushd "$OUTPUT_DIR" >/dev/null
zip -qr "$LIBS_PACKAGE_ZIP" "$LIBS_PROJ_NAME"
if [ $? -ne 0 ]; then echo "ERROR: Failed to create $LIBS_PACKAGE_ZIP ($?)"; exit 1; fi

# Calculate SHA-256
echo "Calculating SHA sum ..."
LIBS_PACKAGE_PATH="$OUTPUT_DIR/$LIBS_PACKAGE_ZIP"
LIBS_PACKAGE_SHA=`shasum -a 256 "$LIBS_PACKAGE_ZIP" | cut -f 1 -d ' '`
LIBS_PACKAGE_SIZE=`get_file_size "$LIBS_PACKAGE_ZIP"`
popd >/dev/null
rm -rf "$LIBS_PKG_DIR"
echo "'$LIBS_PACKAGE_ZIP' Created! Size: $LIBS_PACKAGE_SIZE, SHA-256: $LIBS_PACKAGE_SHA"
echo

# Upload package to release page
echo "Uploading libs package to release page ..."
LIBS_PACKAGE_URL=`git_safe_upload_asset "$LIBS_PACKAGE_PATH"`
echo "Libs Package Uploaded"
echo "Libs Download URL: $LIBS_PACKAGE_URL"
echo

# Construct JQ argument with libs package data
libs_jq_arg="\
    (.packages[0].tools[] | select(.name==\"$LIBS_PROJ_NAME\")).systems[].url = \"$LIBS_PACKAGE_URL\" |\
    (.packages[0].tools[] | select(.name==\"$LIBS_PROJ_NAME\")).systems[].archiveFileName = \"$LIBS_PACKAGE_ZIP\" |\
    (.packages[0].tools[] | select(.name==\"$LIBS_PROJ_NAME\")).systems[].size = \"$LIBS_PACKAGE_SIZE\" |\
    (.packages[0].tools[] | select(.name==\"$LIBS_PROJ_NAME\")).systems[].checksum = \"SHA-256:$LIBS_PACKAGE_SHA\""

# Update template values for the libs package and store it in the build folder
cat "$PACKAGE_JSON_TEMPLATE" | jq "$libs_jq_arg" > "$OUTPUT_DIR/package-$LIBS_PROJ_NAME.json"
# Overwrite the template location with the newly edited one
PACKAGE_JSON_TEMPLATE="$OUTPUT_DIR/package-$LIBS_PROJ_NAME.json"

##
## TEMP WORKAROUND FOR RV32 LONG PATH ON WINDOWS
##
RVTC_VERSION=`cat $PACKAGE_JSON_TEMPLATE | jq -r ".packages[0].platforms[0].toolsDependencies[] | select(.name == \"$RVTC_NAME\") | .version" | cut -d '_' -f 2`
# RVTC_VERSION=`date -j -f '%Y%m%d' "$RVTC_VERSION" '+%y%m'` # MacOS
RVTC_VERSION=`date -d "$RVTC_VERSION" '+%y%m'`
rvtc_jq_arg="\
    (.packages[0].platforms[0].toolsDependencies[] | select(.name==\"$RVTC_NAME\")).version = \"$RVTC_VERSION\" |\
    (.packages[0].platforms[0].toolsDependencies[] | select(.name==\"$RVTC_NAME\")).name = \"$RVTC_NEW_NAME\" |\
    (.packages[0].tools[] | select(.name==\"$RVTC_NAME\")).version = \"$RVTC_VERSION\" |\
    (.packages[0].tools[] | select(.name==\"$RVTC_NAME\")).name = \"$RVTC_NEW_NAME\""
cat "$PACKAGE_JSON_TEMPLATE" | jq "$rvtc_jq_arg" > "$OUTPUT_DIR/package-$LIBS_PROJ_NAME-rvfix.json"
PACKAGE_JSON_TEMPLATE="$OUTPUT_DIR/package-$LIBS_PROJ_NAME-rvfix.json"

##
## PACKAGE JSON
##

# Construct JQ argument with package data
jq_arg=".packages[0].platforms[0].version = \"$RELEASE_TAG\" | \
    .packages[0].platforms[0].url = \"$PACKAGE_URL\" |\
    .packages[0].platforms[0].archiveFileName = \"$PACKAGE_ZIP\" |\
    .packages[0].platforms[0].size = \"$PACKAGE_SIZE\" |\
    .packages[0].platforms[0].checksum = \"SHA-256:$PACKAGE_SHA\""

# Generate package JSONs
echo "Genarating $PACKAGE_JSON_DEV ..."
cat "$PACKAGE_JSON_TEMPLATE" | jq "$jq_arg" > "$OUTPUT_DIR/$PACKAGE_JSON_DEV"
if [ "$RELEASE_PRE" == "false" ]; then
    echo "Genarating $PACKAGE_JSON_REL ..."
    cat "$PACKAGE_JSON_TEMPLATE" | jq "$jq_arg" > "$OUTPUT_DIR/$PACKAGE_JSON_REL"
fi

# Figure out the last release or pre-release
echo "Getting previous releases ..."
releasesJson=`curl -sH "Authorization: token $GITHUB_TOKEN" "https://api.github.com/repos/$GITHUB_REPOSITORY/releases" 2>/dev/null`
if [ $? -ne 0 ]; then echo "ERROR: Get Releases Failed! ($?)"; exit 1; fi

set +e
prev_release=$(echo "$releasesJson" | jq -e -r ". | map(select(.draft == false and .prerelease == false)) | sort_by(.published_at | - fromdateiso8601) | .[0].tag_name")
prev_any_release=$(echo "$releasesJson" | jq -e -r ". | map(select(.draft == false)) | sort_by(.published_at | - fromdateiso8601)  | .[0].tag_name")
shopt -s nocasematch
if [ "$prev_release" == "$RELEASE_TAG" ]; then
    prev_release=$(echo "$releasesJson" | jq -e -r ". | map(select(.draft == false and .prerelease == false)) | sort_by(.published_at | - fromdateiso8601) | .[1].tag_name")
fi
if [ "$prev_any_release" == "$RELEASE_TAG" ]; then
    prev_any_release=$(echo "$releasesJson" | jq -e -r ". | map(select(.draft == false)) | sort_by(.published_at | - fromdateiso8601)  | .[1].tag_name")
fi
shopt -u nocasematch
set -e

echo "Previous Release: $prev_release"
echo "Previous (any)release: $prev_any_release"
echo

# Merge package JSONs with previous releases
if [ ! -z "$prev_any_release" ] && [ "$prev_any_release" != "null" ]; then
    echo "Merging with JSON from $prev_any_release ..."
    merge_package_json "$prev_any_release/$PACKAGE_JSON_DEV" "$OUTPUT_DIR/$PACKAGE_JSON_DEV"
fi

if [ "$RELEASE_PRE" == "false" ]; then
    if [ ! -z "$prev_release" ] && [ "$prev_release" != "null" ]; then
        echo "Merging with JSON from $prev_release ..."
        merge_package_json "$prev_release/$PACKAGE_JSON_REL" "$OUTPUT_DIR/$PACKAGE_JSON_REL"
    fi
fi

# Upload package JSONs
echo "Uploading $PACKAGE_JSON_DEV ..."
echo "Download URL: "`git_safe_upload_asset "$OUTPUT_DIR/$PACKAGE_JSON_DEV"`
echo "Pages URL: "`git_safe_upload_to_pages "$PACKAGE_JSON_DEV" "$OUTPUT_DIR/$PACKAGE_JSON_DEV"`
echo
if [ "$RELEASE_PRE" == "false" ]; then
    echo "Uploading $PACKAGE_JSON_REL ..."
    echo "Download URL: "`git_safe_upload_asset "$OUTPUT_DIR/$PACKAGE_JSON_REL"`
    echo "Pages URL: "`git_safe_upload_to_pages "$PACKAGE_JSON_REL" "$OUTPUT_DIR/$PACKAGE_JSON_REL"`
    echo
fi

set +e

##
## DONE
##
echo "DONE!"
