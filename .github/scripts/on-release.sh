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
PACKAGE_NAME="esp32-core-$RELEASE_TAG"
PACKAGE_JSON_MERGE="$GITHUB_WORKSPACE/.github/scripts/merge_packages.py"
PACKAGE_JSON_TEMPLATE="$GITHUB_WORKSPACE/package/package_esp32_index.template.json"
PACKAGE_JSON_DEV="package_esp32_dev_index.json"
PACKAGE_JSON_REL="package_esp32_index.json"
PACKAGE_JSON_DEV_CN="package_esp32_dev_index_cn.json"
PACKAGE_JSON_REL_CN="package_esp32_index_cn.json"

# Source SoC configuration
source "$SCRIPTS_DIR/socs_config.sh"

# Source common GitHub release functions
source "$SCRIPTS_DIR/lib-github-release.sh"

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

#
# Replace a literal string while skipping the first N occurrences (file-wide).
# Portable across macOS/Linux (avoids sed "skip first match" differences).
#
# skip_n semantics:
#   - skip_n=0 : replace all occurrences (skip none)
#   - skip_n=1 : skip the first occurrence, replace all subsequent ones
#   - skip_n=N : skip the first N occurrences, then replace the rest
#
# Usage:
#   replace_literal_skip_n <skip_n> <from_literal> <to_literal> <infile> <outfile>
#
function replace_literal_skip_n {
    local skip_n="$1"
    local from_literal="$2"
    local to_literal="$3"
    local infile="$4"
    local outfile="$5"

    if [ -z "$infile" ] || [ -z "$outfile" ]; then
        >&2 echo "ERROR: replace_literal_skip_n: missing infile/outfile"
        return 1
    fi

    SKIP="$skip_n" FROM="$from_literal" TO="$to_literal" \
        perl -pe 'BEGIN{$s=$ENV{SKIP}+0;$from=$ENV{FROM};$to=$ENV{TO};$i=0;} s/\Q$from\E/($i++<$s)?$&:$to/ge' \
        "$infile" > "$outfile"
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
if ! "${SCRIPTS_DIR}/update-version.sh" "$RELEASE_TAG"; then
    echo "ERROR: update_version failed!"
    exit 1
fi
git config --global github.user "github-actions[bot]"
git config --global user.name "github-actions[bot]"
git config --global user.email "41898282+github-actions[bot]@users.noreply.github.com"
git add .

# Create version update commit if there are changes
if git diff --cached --quiet; then
    echo "Version already updated"
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
sed 's|{runtime\.platform\.path}/tools/esp32-arduino-libs|{runtime.tools.{build.chip_variant}-libs.path}|g' | \
sed 's|{runtime\.platform\.path}\\tools\\esp32-arduino-libs|{runtime.tools.{build.chip_variant}-libs.path}|g' | \
sed 's|compiler\.sdk\.path={tools\.esp32-arduino-libs\.path}/{build\.chip_variant}|compiler.sdk.path={tools.esp32-arduino-libs.path}|g' | \
sed '/compiler\.sdk\.path\.windows={tools\.esp32-arduino-libs\.path}/d' | \
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
PACKAGE_URL=$(git_safe_upload_asset "$PACKAGE_PATH" "$RELEASE_ID")
echo "Package Uploaded"
echo "Download URL: $PACKAGE_URL"
echo

# Upload XZ package to release page
echo "Uploading XZ package to release page ..."
PACKAGE_XZ_URL=$(git_safe_upload_asset "$PACKAGE_XZ_PATH" "$RELEASE_ID")
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

# Copy esp-hosted binaries

mkdir -p "$GITHUB_WORKSPACE/hosted"
cp "$OUTPUT_DIR/esp32-arduino-libs/hosted"/*.bin "$GITHUB_WORKSPACE/hosted/"

# Create per-SoC ZIPs
echo "Creating per-SoC libs ZIPs..."

declare -A SOC_ZIP_URLS
declare -A SOC_ZIP_FILES
declare -A SOC_CHECKSUMS
declare -A SOC_SIZES

pushd "$OUTPUT_DIR" >/dev/null

for soc_variant in "${CORE_VARIANTS[@]}"; do
    echo "Creating ZIP for $soc_variant..."

    # Create temporary directory structure for this SoC variant
    temp_dir="$soc_variant-libs"
    mkdir -p "$temp_dir"

    # Copy the entire contents of the SoC variant folder (flatten one layer), excluding build files
    if [ -d "esp32-arduino-libs/$soc_variant" ]; then
        for item in "esp32-arduino-libs/$soc_variant"/*; do
            name=$(basename "$item")
            # Skip build/development files
            if [[ "$name" == "dependencies.lock" || \
                  "$name" == "pioarduino-build.py" ]]; then
                continue
            fi
            cp -r "$item" "$temp_dir/"
        done
    else
        echo "ERROR: Variant folder not found: esp32-arduino-libs/$soc_variant"
        exit 1
    fi

    # Create ZIP
    # Arduino Board Manager requires exactly ONE top-level directory in tool archives.
    # Zip the folder itself (not its contents) so it becomes the archive root.
    soc_zip="$soc_variant-libs-$RELEASE_TAG.zip"
    zip -qr "$soc_zip" "$temp_dir"

    # Calculate checksum and size before uploading
    checksum=$(sha256sum "$soc_zip" | awk '{print $1}')
    size=$(stat -c%s "$soc_zip")

    # Clean up temp directory
    rm -rf "$temp_dir"

    # Upload ZIP (S3 if configured, otherwise GitHub)
    if [ "$ENABLE_S3" == "true" ] && [ -n "$S3_BUCKET_NAME" ] && [ -n "$S3_BUCKET_REGION" ]; then
        echo "Uploading $soc_zip to S3..."
        s3_key="$soc_zip"
        aws s3 cp "$OUTPUT_DIR/$soc_zip" "s3://$S3_BUCKET_NAME/arduino/$RELEASE_TAG/$s3_key" --acl public-read --cache-control "public, max-age=31536000, immutable" --content-type application/zip
        soc_zip_url="https://$S3_BUCKET_NAME.s3.$S3_BUCKET_REGION.amazonaws.com/arduino/$RELEASE_TAG/$s3_key"
        echo "$soc_zip Uploaded to S3"
        echo "S3 URL: $soc_zip_url"

        # Upload "latest" version to S3
        latest_zip="${soc_variant}-libs-latest.zip"
        cp "$OUTPUT_DIR/$soc_zip" "$OUTPUT_DIR/$latest_zip"
        echo "Uploading $latest_zip (latest) to S3..."
        aws s3 cp "$OUTPUT_DIR/$latest_zip" "s3://$S3_BUCKET_NAME/arduino/latest/$latest_zip" --acl public-read --cache-control "no-cache, must-revalidate" --content-type application/zip
        latest_zip_url="https://$S3_BUCKET_NAME.s3.$S3_BUCKET_REGION.amazonaws.com/arduino/latest/$latest_zip"
        echo "$latest_zip Uploaded to S3"
        echo "S3 URL: $latest_zip_url"
    else
        echo "Uploading $soc_zip to GitHub releases..."
        soc_zip_url=$(git_safe_upload_asset "$OUTPUT_DIR/$soc_zip" "$RELEASE_ID")
        echo "$soc_zip Uploaded to GitHub"
        echo "GitHub URL: $soc_zip_url"
    fi

    # Store URLs, filenames, checksums and sizes for JSON update
    SOC_ZIP_URLS["$soc_variant"]="$soc_zip_url"
    SOC_ZIP_FILES["$soc_variant"]="$soc_zip"
    SOC_CHECKSUMS["$soc_variant"]="$checksum"
    SOC_SIZES["$soc_variant"]="$size"

    # Clean up ZIP files
    rm -f "$soc_zip"
    if [ "$ENABLE_S3" == "true" ] && [ -n "$S3_BUCKET_NAME" ] && [ -n "$S3_BUCKET_REGION" ]; then
        rm -f "$latest_zip"
    fi
done

popd >/dev/null

echo "Uploading XZ libs to release page ..."
LIBS_XZ_URL=$(git_safe_upload_asset "$OUTPUT_DIR/$LIBS_XZ" "$RELEASE_ID")
echo "XZ libs Uploaded"
echo "Download URL: $LIBS_XZ_URL"
echo

# Update libs URLs in JSON template
echo "Updating libs URLs in JSON template ..."

# Get the existing systems structure from the esp32-arduino-libs tool
# We'll use this as a template for each new SoC tool
existing_systems=$(cat "$PACKAGE_JSON_TEMPLATE" | jq '.packages[0].tools[] | select(.name == "esp32-arduino-libs") | .systems')

# Remove existing esp32-arduino-libs tool and toolsDependency, then add new per-SoC tools and dependencies
jq_args="del(.packages[0].tools[] | select(.name == \"esp32-arduino-libs\"))"
jq_args+=" | del(.packages[0].platforms[0].toolsDependencies[] | select(.name == \"esp32-arduino-libs\"))"

# Iterate over each SoC variant to update the JSON template
for soc_variant in "${CORE_VARIANTS[@]}"; do
    tool_name="${soc_variant}-libs"
    tool_url="${SOC_ZIP_URLS[$soc_variant]}"
    tool_file="${SOC_ZIP_FILES[$soc_variant]}"
    checksum="${SOC_CHECKSUMS[$soc_variant]}"
    size="${SOC_SIZES[$soc_variant]}"

    # Update the systems array with new URL, filename, checksum and size
    updated_systems=$(echo "$existing_systems" | jq --arg url "$tool_url" --arg file "$tool_file" --arg checksum "SHA-256:$checksum" --arg size "$size" '
        map(.url = $url | .archiveFileName = $file | .checksum = $checksum | .size = $size)
    ')

    jq_args+=" | .packages[0].tools += [{\"name\": \"$tool_name\", \"version\": \"${RELEASE_TAG}\", \"systems\": $updated_systems}]"
    jq_args+=" | .packages[0].platforms[0].toolsDependencies += [{\"packager\": \"esp32\", \"name\": \"$tool_name\", \"version\": \"${RELEASE_TAG}\"}]"
done

cat "$PACKAGE_JSON_TEMPLATE" | jq "$jq_args" > "$OUTPUT_DIR/package-libs-updated.json"
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
tmp_pkg_json="$OUTPUT_DIR/${PACKAGE_JSON_DEV}.raw"
cat "$PACKAGE_JSON_TEMPLATE" | jq "$jq_arg" > "$tmp_pkg_json"
# For some reason downloads from dl.espressif.com keep failing. Commenting out for now.
# replace_literal_skip_n 2 "github.com/" "dl.espressif.com/github_assets/" "$tmp_pkg_json" "$OUTPUT_DIR/$PACKAGE_JSON_DEV"
cp "$tmp_pkg_json" "$OUTPUT_DIR/$PACKAGE_JSON_DEV" # Can remove this once we have a fix for dl.espressif.com
replace_literal_skip_n 1 "github.com/" "dl.espressif.cn/github_assets/" "$tmp_pkg_json" "$OUTPUT_DIR/$PACKAGE_JSON_DEV_CN"
rm -f "$tmp_pkg_json"
python "$SCRIPTS_DIR/release_append_cn.py" "$OUTPUT_DIR/$PACKAGE_JSON_DEV_CN"
if [ "$RELEASE_PRE" == "false" ]; then
    echo "Generating $PACKAGE_JSON_REL ..."
    tmp_pkg_json="$OUTPUT_DIR/${PACKAGE_JSON_REL}.raw"
    cat "$PACKAGE_JSON_TEMPLATE" | jq "$jq_arg" > "$tmp_pkg_json"
    # For some reason downloads from dl.espressif.com keep failing. Commenting out for now.
    # replace_literal_skip_n 2 "github.com/" "dl.espressif.com/github_assets/" "$tmp_pkg_json" "$OUTPUT_DIR/$PACKAGE_JSON_REL"
    cp "$tmp_pkg_json" "$OUTPUT_DIR/$PACKAGE_JSON_REL" # Can remove this once we have a fix for dl.espressif.com
    replace_literal_skip_n 1 "github.com/" "dl.espressif.cn/github_assets/" "$tmp_pkg_json" "$OUTPUT_DIR/$PACKAGE_JSON_REL_CN"
    rm -f "$tmp_pkg_json"
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

##
## Build complete - testing and uploading will be handled by GitHub Actions workflow
##
echo "Build complete! Package JSONs generated."
echo "- $PACKAGE_JSON_DEV"
echo "- $PACKAGE_JSON_DEV_CN"
if [ "$RELEASE_PRE" == "false" ]; then
    echo "- $PACKAGE_JSON_REL"
    echo "- $PACKAGE_JSON_REL_CN"
fi
