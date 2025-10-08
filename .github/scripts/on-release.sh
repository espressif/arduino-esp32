#!/bin/bash
# Disable shellcheck warning about using 'cat' to read a file.
# Disable shellcheck warning about using individual redirections for each command.
# Disable shellcheck warning about $? uses.
# shellcheck disable=SC2002,SC2129,SC2181,SC2319

if [ ! "$GITHUB_EVENT_NAME" == "release" ]; then
    echo "Wrong event '$GITHUB_EVENT_NAME'!"
    exit 1
fi

EVENT_JSON=$(cat "$GITHUB_EVENT_PATH")

action=$(echo "$EVENT_JSON" | jq -r '.action')
if [ ! "$action" == "published" ]; then
    echo "Wrong action '$action'. Exiting now..."
    exit 0
fi

draft=$(echo "$EVENT_JSON" | jq -r '.release.draft')
if [ "$draft" == "true" ]; then
    echo "It's a draft release. Exiting now..."
    exit 0
fi

RELEASE_PRE=$(echo "$EVENT_JSON" | jq -r '.release.prerelease')
RELEASE_TAG=$(echo "$EVENT_JSON" | jq -r '.release.tag_name')
RELEASE_BRANCH=$(echo "$EVENT_JSON" | jq -r '.release.target_commitish')
RELEASE_ID=$(echo "$EVENT_JSON" | jq -r '.release.id')

SCRIPTS_DIR="./.github/scripts"
OUTPUT_DIR="$GITHUB_WORKSPACE/build"
PACKAGE_NAME="esp32-$RELEASE_TAG"
PACKAGE_JSON_MERGE="$GITHUB_WORKSPACE/.github/scripts/merge_packages.py"
PACKAGE_JSON_TEMPLATE="$GITHUB_WORKSPACE/package/package_esp32_index.template.json"
PACKAGE_JSON_DEV="package_esp32_dev_index.json"
PACKAGE_JSON_REL="package_esp32_index.json"
PACKAGE_JSON_DEV_CN="package_esp32_dev_index_cn.json"
PACKAGE_JSON_REL_CN="package_esp32_index_cn.json"

echo "Event: $GITHUB_EVENT_NAME, Repo: $GITHUB_REPOSITORY, Path: $GITHUB_WORKSPACE, Ref: $GITHUB_REF"
echo "Action: $action, Branch: $RELEASE_BRANCH, ID: $RELEASE_ID"
echo "Tag: $RELEASE_TAG, Draft: $draft, Pre-Release: $RELEASE_PRE"

# Try extracting something like a JSON with a "boards" array/element and "vendor" fields
BOARDS=$(echo "$RELEASE_BODY" | grep -Pzo '(?s){.*}' | jq -r '.boards[]? // .boards? // empty' | xargs echo -n 2>/dev/null)
VENDOR=$(echo "$RELEASE_BODY" | grep -Pzo '(?s){.*}' | jq -r '.vendor? // empty' | xargs echo -n 2>/dev/null)

if [ -n "${BOARDS}" ]; then
    echo "Releasing board(s): $BOARDS"
fi

if [ -n "${VENDOR}" ]; then
    echo "Setting packager: $VENDOR"
fi

function update_version {
    set -e
    set -o pipefail

    local tag=$1
    local major
    local minor
    local patch

    # Extract major, minor, and patch from the tag
    # We need to make sure to remove the "v" prefix from the tag and any characters after the patch version
    tag=$(echo "$tag" | sed 's/^v//g' | sed 's/-.*//g')
    major=$(echo "$tag" | cut -d. -f1)
    minor=$(echo "$tag" | cut -d. -f2)
    patch=$(echo "$tag" | cut -d. -f3 | sed 's/[^0-9].*//')  # Remove non-numeric suffixes like RC1, alpha, beta

    echo "Major: $major, Minor: $minor, Patch: $patch"

    "${SCRIPTS_DIR}/update-version.sh" "$major" "$minor" "$patch"

    set +e
    set +o pipefail
}

function get_file_size {
    local file="$1"
    if [[ "$OSTYPE" == "darwin"* ]]; then
        eval "$(stat -s "$file")"
        local res="$?"
        echo "${st_size:?}"
        return $res
    else
        stat --printf="%s" "$file"
        return $?
    fi
}

function git_upload_asset {
    local name
    name=$(basename "$1")
    # local mime=$(file -b --mime-type "$1")
    curl -k -X POST -sH "Authorization: token $GITHUB_TOKEN" -H "Content-Type: application/octet-stream" --data-binary @"$1" "https://uploads.github.com/repos/$GITHUB_REPOSITORY/releases/$RELEASE_ID/assets?name=$name"
}

function git_safe_upload_asset {
    local file="$1"
    local name
    local size
    local upload_res

    name=$(basename "$file")
    size=$(get_file_size "$file")

    if ! upload_res=$(git_upload_asset "$file"); then
        >&2 echo "ERROR: Failed to upload '$name' ($?)"
        return 1
    fi

    up_size=$(echo "$upload_res" | jq -r '.size')
    if [ "$up_size" -ne "$size" ]; then
        >&2 echo "ERROR: Uploaded size does not match! $up_size != $size"
        #git_delete_asset
        return 1
    fi
    echo "$upload_res" | jq -r '.browser_download_url'
    return $?
}

function git_upload_to_pages {
    local path=$1
    local src=$2

    if [ ! -f "$src" ]; then
        >&2 echo "Input is not a file! Aborting..."
        return 1
    fi

    local info
    local type
    local message
    local sha=""
    local content=""

    info=$(curl -s -k -H "Authorization: token $GITHUB_TOKEN" -H "Accept: application/vnd.github.v3.object+json" -X GET "https://api.github.com/repos/$GITHUB_REPOSITORY/contents/$path?ref=gh-pages")
    type=$(echo "$info" | jq -r '.type')
    message=$(basename "$path")

    if [ "$type" == "file" ]; then
        sha=$(echo "$info" | jq -r '.sha')
        sha=",\"sha\":\"$sha\""
        message="Updating $message"
    elif [ ! "$type" == "null" ]; then
        >&2 echo "Wrong type '$type'"
        return 1
    else
        message="Creating $message"
    fi

    content=$(base64 -i "$src")
    data="{\"branch\":\"gh-pages\",\"message\":\"$message\",\"content\":\"$content\"$sha}"

    echo "$data" | curl -s -k -H "Authorization: token $GITHUB_TOKEN" -H "Accept: application/vnd.github.v3.raw+json" -X PUT --data @- "https://api.github.com/repos/$GITHUB_REPOSITORY/contents/$path"
}

function git_safe_upload_to_pages {
    local path=$1
    local file="$2"
    local name
    local size
    local upload_res

    name=$(basename "$file")
    size=$(get_file_size "$file")

    if ! upload_res=$(git_upload_to_pages "$path" "$file"); then
        >&2 echo "ERROR: Failed to upload '$name' ($?)"
        return 1
    fi

    up_size=$(echo "$upload_res" | jq -r '.content.size')
    if [ "$up_size" -ne "$size" ]; then
        >&2 echo "ERROR: Uploaded size does not match! $up_size != $size"
        #git_delete_asset
        return 1
    fi
    echo "$upload_res" | jq -r '.content.download_url'
    return $?
}

function merge_package_json {
    local jsonLink=$1
    local jsonOut=$2
    local old_json=$OUTPUT_DIR/oldJson.json
    local merged_json=$OUTPUT_DIR/mergedJson.json
    local error_code=0

    echo "Downloading previous JSON $jsonLink ..."
    curl -L -o "$old_json" "https://github.com/$GITHUB_REPOSITORY/releases/download/$jsonLink?access_token=$GITHUB_TOKEN" 2>/dev/null
    error_code=$?
    if [ $error_code -ne 0 ]; then
        echo "ERROR: Download Failed! $error_code"
        exit 1
    fi

    echo "Creating new JSON ..."
    set +e
    stdbuf -oL python "$PACKAGE_JSON_MERGE" "$jsonOut" "$old_json" > "$merged_json"
    set -e

    set -v
    if [ ! -s "$merged_json" ]; then
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
PKG_DIR="${OUTPUT_DIR:?}/$PACKAGE_NAME"
PACKAGE_ZIP="$PACKAGE_NAME.zip"
PACKAGE_XZ="$PACKAGE_NAME.tar.xz"
LIBS_ZIP="$PACKAGE_NAME-libs.zip"
LIBS_XZ="$PACKAGE_NAME-libs.tar.xz"

echo "Updating version..."
if ! update_version "$RELEASE_TAG"; then
    echo "ERROR: update_version failed!"
    exit 1
fi
git config --global github.user "github-actions[bot]"
git config --global user.name "github-actions[bot]"
git config --global user.email "41898282+github-actions[bot]@users.noreply.github.com"
git add .

# We should only commit if there are changes
need_update_commit=true
if git diff --cached --quiet; then
    echo "Version already updated"
    need_update_commit=false
else
    echo "Creating version update commit..."
    git commit -m "change(version): Update core version to $RELEASE_TAG"
fi

echo "Updating submodules ..."
git -C "$GITHUB_WORKSPACE" submodule update --init --recursive > /dev/null 2>&1

mkdir -p "$PKG_DIR/tools"

# Copy all core files to the package folder
echo "Copying files for packaging ..."
if [ -z "${BOARDS}" ]; then
    # Copy all variants
    cp -f  "$GITHUB_WORKSPACE/boards.txt"                   "$PKG_DIR/"
    cp -Rf "$GITHUB_WORKSPACE/variants"                     "$PKG_DIR/"
else
    # Remove all entries not starting with any board code or "menu." from boards.txt
    cat "$GITHUB_WORKSPACE/boards.txt" | grep "^menu\."         >  "$PKG_DIR/boards.txt"
    for board in ${BOARDS} ; do
        cat "$GITHUB_WORKSPACE/boards.txt" | grep "^${board}\." >> "$PKG_DIR/boards.txt"
    done
    # Copy only relevant variant files
    mkdir "$PKG_DIR/variants/"
    board_list=$(cat "${PKG_DIR}"/boards.txt | grep "\.variant=" | cut -d= -f2)
    while IFS= read -r variant; do
        cp -Rf "$GITHUB_WORKSPACE/variants/${variant}"      "$PKG_DIR/variants/"
    done <<< "$board_list"
fi
cp -f  "$GITHUB_WORKSPACE/CMakeLists.txt"                   "$PKG_DIR/"
cp -f  "$GITHUB_WORKSPACE/idf_component.yml"                "$PKG_DIR/"
cp -f  "$GITHUB_WORKSPACE/Kconfig.projbuild"                "$PKG_DIR/"
cp -f  "$GITHUB_WORKSPACE/package.json"                     "$PKG_DIR/"
cp -f  "$GITHUB_WORKSPACE/programmers.txt"                  "$PKG_DIR/"
cp -Rf "$GITHUB_WORKSPACE/cores"                            "$PKG_DIR/"
cp -Rf "$GITHUB_WORKSPACE/libraries"                        "$PKG_DIR/"
cp -f  "$GITHUB_WORKSPACE/tools/espota.exe"                 "$PKG_DIR/tools/"
cp -f  "$GITHUB_WORKSPACE/tools/espota.py"                  "$PKG_DIR/tools/"
cp -f  "$GITHUB_WORKSPACE/tools/gen_esp32part.py"           "$PKG_DIR/tools/"
cp -f  "$GITHUB_WORKSPACE/tools/gen_esp32part.exe"          "$PKG_DIR/tools/"
cp -f  "$GITHUB_WORKSPACE/tools/gen_insights_package.py"    "$PKG_DIR/tools/"
cp -f  "$GITHUB_WORKSPACE/tools/gen_insights_package.exe"   "$PKG_DIR/tools/"
cp -Rf "$GITHUB_WORKSPACE/tools/partitions"                 "$PKG_DIR/tools/"
cp -Rf "$GITHUB_WORKSPACE/tools/ide-debug"                  "$PKG_DIR/tools/"
cp -f  "$GITHUB_WORKSPACE/tools/pioarduino-build.py"        "$PKG_DIR/tools/"

# Remove unnecessary files in the package folder
echo "Cleaning up folders ..."
find "$PKG_DIR" -name '*.DS_Store' -exec rm -f {} \;
find "$PKG_DIR" -name '*.git*' -type f -delete

##
## TEMP WORKAROUND FOR RV32 LONG PATH ON WINDOWS
##
RVTC_NAME="riscv32-esp-elf-gcc"
RVTC_NEW_NAME="esp-rv32"
X32TC_NAME="xtensa-esp-elf-gcc"
X32TC_NEW_NAME="esp-x32"

# Replace tools locations in platform.txt
echo "Generating platform.txt..."
cat "$GITHUB_WORKSPACE/platform.txt" | \
sed "s/version=.*/version=$RELEASE_TAG/g" | \
sed 's/tools\.esp32-arduino-libs\.path\.windows=.*//g' | \
sed 's/{runtime\.platform\.path}.tools.esp32-arduino-libs/\{runtime.tools.esp32-arduino-libs.path\}/g' | \
sed 's/{runtime\.platform\.path}.tools.xtensa-esp-elf-gdb/\{runtime.tools.xtensa-esp-elf-gdb.path\}/g' | \
sed "s/{runtime\.platform\.path}.tools.xtensa-esp-elf/\\{runtime.tools.$X32TC_NEW_NAME.path\\}/g" | \
sed 's/{runtime\.platform\.path}.tools.riscv32-esp-elf-gdb/\{runtime.tools.riscv32-esp-elf-gdb.path\}/g' | \
sed "s/{runtime\.platform\.path}.tools.riscv32-esp-elf/\\{runtime.tools.$RVTC_NEW_NAME.path\\}/g" | \
sed 's/{runtime\.platform\.path}.tools.esptool/\{runtime.tools.esptool_py.path\}/g' | \
sed 's/{runtime\.platform\.path}.tools.openocd-esp32/\{runtime.tools.openocd-esp32.path\}/g' > "$PKG_DIR/platform.txt"

if [ -n "${VENDOR}" ]; then
    # Append vendor name to platform.txt to create a separate section
    sed -i  "/^name=.*/s/$/ ($VENDOR)/" "$PKG_DIR/platform.txt"
fi

# Add header with version information
echo "Generating core_version.h ..."
ver_define=$(echo "$RELEASE_TAG" | tr "[:lower:].\055" "[:upper:]_")
ver_hex=$(git -C "$GITHUB_WORKSPACE" rev-parse --short=8 HEAD 2>/dev/null)
echo \#define ARDUINO_ESP32_GIT_VER 0x"$ver_hex" > "$PKG_DIR/cores/esp32/core_version.h"
echo \#define ARDUINO_ESP32_GIT_DESC "$(git -C "$GITHUB_WORKSPACE" describe --tags 2>/dev/null)" >> "$PKG_DIR/cores/esp32/core_version.h"
echo \#define ARDUINO_ESP32_RELEASE_"$ver_define" >> "$PKG_DIR/cores/esp32/core_version.h"
echo \#define ARDUINO_ESP32_RELEASE \""$ver_define"\" >> "$PKG_DIR/cores/esp32/core_version.h"

# Compress ZIP package folder
echo "Creating ZIP ..."
pushd "$OUTPUT_DIR" >/dev/null
zip -qr "$PACKAGE_ZIP" "$PACKAGE_NAME"
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to create $PACKAGE_ZIP ($?)"
    exit 1
fi

# Calculate SHA-256
echo "Calculating ZIP SHA sum ..."
PACKAGE_PATH="${OUTPUT_DIR:?}/$PACKAGE_ZIP"
PACKAGE_SHA=$(shasum -a 256 "$PACKAGE_ZIP" | cut -f 1 -d ' ')
PACKAGE_SIZE=$(get_file_size "$PACKAGE_ZIP")
popd >/dev/null
echo "'$PACKAGE_ZIP' Created! Size: $PACKAGE_SIZE, SHA-256: $PACKAGE_SHA"
echo

# Compress XZ package folder
echo "Creating XZ ..."
pushd "$OUTPUT_DIR" >/dev/null
tar -cJf "$PACKAGE_XZ" "$PACKAGE_NAME"
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to create $PACKAGE_XZ ($?)"
    exit 1
fi

# Calculate SHA-256
echo "Calculating XZ SHA sum ..."
PACKAGE_XZ_PATH="${OUTPUT_DIR:?}/$PACKAGE_XZ"
PACKAGE_XZ_SHA=$(shasum -a 256 "$PACKAGE_XZ" | cut -f 1 -d ' ')
PACKAGE_XZ_SIZE=$(get_file_size "$PACKAGE_XZ")
popd >/dev/null
echo "'$PACKAGE_XZ' Created! Size: $PACKAGE_XZ_SIZE, SHA-256: $PACKAGE_XZ_SHA"
echo

# Upload ZIP package to release page
echo "Uploading ZIP package to release page ..."
PACKAGE_URL=$(git_safe_upload_asset "$PACKAGE_PATH")
echo "Package Uploaded"
echo "Download URL: $PACKAGE_URL"
echo

# Upload XZ package to release page
echo "Uploading XZ package to release page ..."
PACKAGE_XZ_URL=$(git_safe_upload_asset "$PACKAGE_XZ_PATH")
echo "Package Uploaded"
echo "Download URL: $PACKAGE_XZ_URL"
echo

# Remove package folder
rm -rf "$PKG_DIR"

# Copy Libs from lib-builder to release in ZIP and XZ

libs_url=$(cat "$PACKAGE_JSON_TEMPLATE" | jq -r ".packages[0].tools[] | select(.name == \"esp32-arduino-libs\") | .systems[0].url")
libs_sha=$(cat "$PACKAGE_JSON_TEMPLATE" | jq -r ".packages[0].tools[] | select(.name == \"esp32-arduino-libs\") | .systems[0].checksum" | sed 's/^SHA-256://')
libs_size=$(cat "$PACKAGE_JSON_TEMPLATE" | jq -r ".packages[0].tools[] | select(.name == \"esp32-arduino-libs\") | .systems[0].size")
echo "Downloading libs from lib-builder ..."
echo "URL: $libs_url"
echo "Expected SHA: $libs_sha"
echo "Expected Size: $libs_size"
echo

echo "Downloading libs from lib-builder ..."
curl -L -o "$OUTPUT_DIR/$LIBS_ZIP" "$libs_url"

# Check SHA and Size
zip_sha=$(sha256sum "$OUTPUT_DIR/$LIBS_ZIP" | awk '{print $1}')
zip_size=$(stat -c%s "$OUTPUT_DIR/$LIBS_ZIP")
echo "Downloaded SHA: $zip_sha"
echo "Downloaded Size: $zip_size"
if [ "$zip_sha" != "$libs_sha" ] || [ "$zip_size" != "$libs_size" ]; then
    echo "ERROR: ZIP SHA and Size do not match"
    exit 1
fi

# Extract ZIP

echo "Repacking libs to XZ ..."
unzip -q "$OUTPUT_DIR/$LIBS_ZIP" -d "$OUTPUT_DIR"
pushd "$OUTPUT_DIR" >/dev/null
tar -cJf "$LIBS_XZ" "esp32-arduino-libs"
popd >/dev/null

# Upload ZIP and XZ libs to release page

echo "Uploading ZIP libs to release page ..."
LIBS_ZIP_URL=$(git_safe_upload_asset "$OUTPUT_DIR/$LIBS_ZIP")
echo "ZIP libs Uploaded"
echo "Download URL: $LIBS_ZIP_URL"
echo

echo "Uploading XZ libs to release page ..."
LIBS_XZ_URL=$(git_safe_upload_asset "$OUTPUT_DIR/$LIBS_XZ")
echo "XZ libs Uploaded"
echo "Download URL: $LIBS_XZ_URL"
echo

# Update libs URLs in JSON template
echo "Updating libs URLs in JSON template ..."

# Update all libs URLs in the JSON template
libs_jq_arg="(.packages[0].tools[] | select(.name == \"esp32-arduino-libs\") | .systems[].url) = \"$LIBS_ZIP_URL\" |\
             (.packages[0].tools[] | select(.name == \"esp32-arduino-libs\") | .systems[].archiveFileName) = \"$LIBS_ZIP\""

cat "$PACKAGE_JSON_TEMPLATE" | jq "$libs_jq_arg" > "$OUTPUT_DIR/package-libs-updated.json"
PACKAGE_JSON_TEMPLATE="$OUTPUT_DIR/package-libs-updated.json"

echo "Libs URLs updated in JSON template"
echo

# Clean up
rm -rf "${OUTPUT_DIR:?}/esp32-arduino-libs"
rm -rf "${OUTPUT_DIR:?}/$LIBS_ZIP"
rm -rf "${OUTPUT_DIR:?}/$LIBS_XZ"

##
## TEMP WORKAROUND FOR RV32 LONG PATH ON WINDOWS
##
RVTC_VERSION=$(cat "$PACKAGE_JSON_TEMPLATE" | jq -r ".packages[0].platforms[0].toolsDependencies[] | select(.name == \"$RVTC_NAME\") | .version" | cut -d '_' -f 2)
# RVTC_VERSION=`date -j -f '%Y%m%d' "$RVTC_VERSION" '+%y%m'` # MacOS
RVTC_VERSION=$(date -d "$RVTC_VERSION" '+%y%m')
rvtc_jq_arg="\
    (.packages[0].platforms[0].toolsDependencies[] | select(.name==\"$RVTC_NAME\")).version = \"$RVTC_VERSION\" |\
    (.packages[0].platforms[0].toolsDependencies[] | select(.name==\"$RVTC_NAME\")).name = \"$RVTC_NEW_NAME\" |\
    (.packages[0].tools[] | select(.name==\"$RVTC_NAME\")).version = \"$RVTC_VERSION\" |\
    (.packages[0].tools[] | select(.name==\"$RVTC_NAME\")).name = \"$RVTC_NEW_NAME\" |\
    (.packages[0].platforms[0].toolsDependencies[] | select(.name==\"$X32TC_NAME\")).version = \"$RVTC_VERSION\" |\
    (.packages[0].platforms[0].toolsDependencies[] | select(.name==\"$X32TC_NAME\")).name = \"$X32TC_NEW_NAME\" |\
    (.packages[0].tools[] | select(.name==\"$X32TC_NAME\")).version = \"$RVTC_VERSION\" |\
    (.packages[0].tools[] | select(.name==\"$X32TC_NAME\")).name = \"$X32TC_NEW_NAME\""
cat "$PACKAGE_JSON_TEMPLATE" | jq "$rvtc_jq_arg" > "$OUTPUT_DIR/package-rvfix.json"
PACKAGE_JSON_TEMPLATE="$OUTPUT_DIR/package-rvfix.json"

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
echo "Generating $PACKAGE_JSON_DEV ..."
cat "$PACKAGE_JSON_TEMPLATE" | jq "$jq_arg" > "$OUTPUT_DIR/$PACKAGE_JSON_DEV"
# On MacOS the sed command won't skip the first match. Use gsed instead.
sed '0,/github\.com\//!s|github\.com/|dl.espressif.cn/github_assets/|g' "$OUTPUT_DIR/$PACKAGE_JSON_DEV" > "$OUTPUT_DIR/$PACKAGE_JSON_DEV_CN"
python "$SCRIPTS_DIR/release_append_cn.py" "$OUTPUT_DIR/$PACKAGE_JSON_DEV_CN"
if [ "$RELEASE_PRE" == "false" ]; then
    echo "Generating $PACKAGE_JSON_REL ..."
    cat "$PACKAGE_JSON_TEMPLATE" | jq "$jq_arg" > "$OUTPUT_DIR/$PACKAGE_JSON_REL"
    # On MacOS the sed command won't skip the first match. Use gsed instead.
    sed '0,/github\.com\//!s|github\.com/|dl.espressif.cn/github_assets/|g' "$OUTPUT_DIR/$PACKAGE_JSON_REL" > "$OUTPUT_DIR/$PACKAGE_JSON_REL_CN"
    python "$SCRIPTS_DIR/release_append_cn.py" "$OUTPUT_DIR/$PACKAGE_JSON_REL_CN"
fi

# Figure out the last release or pre-release
echo "Getting previous releases ..."
releasesJson=$(curl -sH "Authorization: token $GITHUB_TOKEN" "https://api.github.com/repos/$GITHUB_REPOSITORY/releases" 2>/dev/null)
if [ $? -ne 0 ]; then
    echo "ERROR: Get Releases Failed! ($?)"
    exit 1
fi

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
if [ -n "$prev_any_release" ] && [ "$prev_any_release" != "null" ]; then
    echo "Merging with JSON from $prev_any_release ..."
    merge_package_json "$prev_any_release/$PACKAGE_JSON_DEV" "$OUTPUT_DIR/$PACKAGE_JSON_DEV"
    merge_package_json "$prev_any_release/$PACKAGE_JSON_DEV_CN" "$OUTPUT_DIR/$PACKAGE_JSON_DEV_CN"
fi

if [ "$RELEASE_PRE" == "false" ]; then
    if [ -n "$prev_release" ] && [ "$prev_release" != "null" ]; then
        echo "Merging with JSON from $prev_release ..."
        merge_package_json "$prev_release/$PACKAGE_JSON_REL" "$OUTPUT_DIR/$PACKAGE_JSON_REL"
        merge_package_json "$prev_release/$PACKAGE_JSON_REL_CN" "$OUTPUT_DIR/$PACKAGE_JSON_REL_CN"
    fi
fi

# Test the package JSONs

echo "Installing arduino-cli ..."
export PATH="/home/runner/bin:$PATH"
source "${SCRIPTS_DIR}/install-arduino-cli.sh"

# For the Chinese mirror, we can't test the package JSONs as the Chinese mirror might not be updated yet.

echo "Testing $PACKAGE_JSON_DEV install ..."

echo "Installing esp32 ..."
arduino-cli core install esp32:esp32 --additional-urls "file://$OUTPUT_DIR/$PACKAGE_JSON_DEV"
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to install esp32 ($?)"
    exit 1
fi

echo "Compiling example ..."
arduino-cli compile --fqbn esp32:esp32:esp32 "$GITHUB_WORKSPACE"/libraries/ESP32/examples/CI/CIBoardsTest/CIBoardsTest.ino
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to compile example ($?)"
    exit 1
fi

echo "Uninstalling esp32 ..."
arduino-cli core uninstall esp32:esp32
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to uninstall esp32 ($?)"
    exit 1
fi

echo "Test successful!"

if [ "$RELEASE_PRE" == "false" ]; then
    echo "Testing $PACKAGE_JSON_REL install ..."

    echo "Installing esp32 ..."
    arduino-cli core install esp32:esp32 --additional-urls "file://$OUTPUT_DIR/$PACKAGE_JSON_REL"
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to install esp32 ($?)"
        exit 1
    fi

    echo "Compiling example ..."
    arduino-cli compile --fqbn esp32:esp32:esp32 "$GITHUB_WORKSPACE"/libraries/ESP32/examples/CI/CIBoardsTest/CIBoardsTest.ino
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to compile example ($?)"
        exit 1
    fi

    echo "Uninstalling esp32 ..."
    arduino-cli core uninstall esp32:esp32
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to uninstall esp32 ($?)"
        exit 1
    fi

    echo "Test successful!"
fi

# Upload package JSONs

echo "Uploading $PACKAGE_JSON_DEV ..."
echo "Download URL: $(git_safe_upload_asset "$OUTPUT_DIR/$PACKAGE_JSON_DEV")"
echo "Pages URL: $(git_safe_upload_to_pages "$PACKAGE_JSON_DEV" "$OUTPUT_DIR/$PACKAGE_JSON_DEV")"
echo "Download CN URL: $(git_safe_upload_asset "$OUTPUT_DIR/$PACKAGE_JSON_DEV_CN")"
echo "Pages CN URL: $(git_safe_upload_to_pages "$PACKAGE_JSON_DEV_CN" "$OUTPUT_DIR/$PACKAGE_JSON_DEV_CN")"
echo
if [ "$RELEASE_PRE" == "false" ]; then
    echo "Uploading $PACKAGE_JSON_REL ..."
    echo "Download URL: $(git_safe_upload_asset "$OUTPUT_DIR/$PACKAGE_JSON_REL")"
    echo "Pages URL: $(git_safe_upload_to_pages "$PACKAGE_JSON_REL" "$OUTPUT_DIR/$PACKAGE_JSON_REL")"
    echo "Download CN URL: $(git_safe_upload_asset "$OUTPUT_DIR/$PACKAGE_JSON_REL_CN")"
    echo "Pages CN URL: $(git_safe_upload_to_pages "$PACKAGE_JSON_REL_CN" "$OUTPUT_DIR/$PACKAGE_JSON_REL_CN")"
    echo
fi

if [ "$need_update_commit" == "true" ]; then
    echo "Pushing version update commit..."
    git push
    new_tag_commit=$(git rev-parse HEAD)
    echo "New commit: $new_tag_commit"

    echo "Moving tag $RELEASE_TAG to $new_tag_commit..."
    git tag -f "$RELEASE_TAG" "$new_tag_commit"
    git push --force origin "$RELEASE_TAG"
fi

set +e

##
## DONE
##
echo "DONE!"
