#!/bin/bash
# Build release packages locally (no GitHub release uploads).
# shellcheck disable=SC2129,SC2181,SC2319

set -e

RELEASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPTS_DIR="$(cd "$RELEASE_DIR/.." && pwd)"
GITHUB_WORKSPACE="${GITHUB_WORKSPACE:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"

if [ -z "${RELEASE_TAG:-}" ]; then
    echo "ERROR: RELEASE_TAG is required" >&2
    exit 1
fi

RELEASE_TAG=$(echo "$RELEASE_TAG" | sed 's/^v//')
RELEASE_PRE="${RELEASE_PRE:-false}"
BOARDS="${BOARDS:-}"
VENDOR="${VENDOR:-}"

source "$SCRIPTS_DIR/socs_config.sh"
source "$RELEASE_DIR/common.sh"

# Local validation only needs ZIPs; skip slow xz repacks unless explicitly enabled.
if [ -z "${SKIP_XZ_GENERATION+x}" ]; then
    if [ -n "${GITHUB_ACTIONS:-}" ]; then
        SKIP_XZ_GENERATION=false
    else
        SKIP_XZ_GENERATION=true
    fi
fi

empty_release_output_dir

PACKAGE_NAME="esp32-core-$RELEASE_TAG"
PACKAGE_JSON_TEMPLATE="$GITHUB_WORKSPACE/package/package_esp32_index.template.json"

echo "Building release packages for $RELEASE_TAG (prerelease=$RELEASE_PRE)"
echo "Output directory: $OUTPUT_DIR"
echo "SKIP_XZ_GENERATION=$SKIP_XZ_GENERATION"
PKG_DIR="${OUTPUT_DIR}/$PACKAGE_NAME"
PACKAGE_ZIP="$PACKAGE_NAME.zip"
PACKAGE_XZ="$PACKAGE_NAME.tar.xz"
LIBS_ZIP="$PACKAGE_NAME-libs.zip"
LIBS_XZ="$PACKAGE_NAME-libs.tar.xz"

echo "Bumping version in tree (no git commit until release is published)..."
if ! "${SCRIPTS_DIR}/update-version.sh" "$RELEASE_TAG"; then
    echo "ERROR: update_version failed!"
    exit 1
fi

echo "Updating submodules ..."
git -C "$GITHUB_WORKSPACE" submodule update --init --recursive > /dev/null 2>&1

mkdir -p "$PKG_DIR/tools"

echo "Copying files for packaging ..."
if [ -z "${BOARDS}" ]; then
    cp -f  "$GITHUB_WORKSPACE/boards.txt"                   "$PKG_DIR/"
    cp -Rf "$GITHUB_WORKSPACE/variants"                     "$PKG_DIR/"
else
    cat "$GITHUB_WORKSPACE/boards.txt" | grep "^menu\."         >  "$PKG_DIR/boards.txt"
    for board in ${BOARDS} ; do
        cat "$GITHUB_WORKSPACE/boards.txt" | grep "^${board}\." >> "$PKG_DIR/boards.txt"
    done
    mkdir "$PKG_DIR/variants/"
    board_list=$(grep "\.variant=" "${PKG_DIR}/boards.txt" | cut -d= -f2)
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

echo "Copying tools root scripts ..."
shopt -s nullglob
for tool_file in "$GITHUB_WORKSPACE"/tools/*.py "$GITHUB_WORKSPACE"/tools/*.exe; do
    tool_base="${tool_file##*/}"
    case "$tool_base" in
        get.*|arduino_cmake.*) continue ;;
        *) echo "Copying $tool_file" ;;
    esac
    cp -f "$tool_file" "$PKG_DIR/tools/"
done
shopt -u nullglob

cp -Rf "$GITHUB_WORKSPACE/tools/partitions"                 "$PKG_DIR/tools/"
cp -Rf "$GITHUB_WORKSPACE/tools/ide-debug"                  "$PKG_DIR/tools/"

echo "Cleaning up folders ..."
find "$PKG_DIR" -name '*.DS_Store' -exec rm -f {} \;
find "$PKG_DIR" -name '*.git*' -type f -delete

RVTC_NAME="riscv32-esp-elf-gcc"
RVTC_NEW_NAME="esp-rv32"
X32TC_NAME="xtensa-esp-elf-gcc"
X32TC_NEW_NAME="esp-x32"

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
    sed -i.bak "/^name=.*/s/$/ ($VENDOR)/" "$PKG_DIR/platform.txt"
    rm -f "$PKG_DIR/platform.txt.bak"
fi

echo "Generating core_version.h ..."
ver_define=$(echo "$RELEASE_TAG" | tr "[:lower:].\055" "[:upper:]_")
ver_hex=$(git -C "$GITHUB_WORKSPACE" rev-parse --short=8 HEAD 2>/dev/null)
echo \#define ARDUINO_ESP32_GIT_VER 0x"$ver_hex" > "$PKG_DIR/cores/esp32/core_version.h"
echo \#define ARDUINO_ESP32_GIT_DESC "$(git -C "$GITHUB_WORKSPACE" describe --tags 2>/dev/null)" >> "$PKG_DIR/cores/esp32/core_version.h"
echo \#define ARDUINO_ESP32_RELEASE_"$ver_define" >> "$PKG_DIR/cores/esp32/core_version.h"
echo \#define ARDUINO_ESP32_RELEASE \""$ver_define"\" >> "$PKG_DIR/cores/esp32/core_version.h"

echo "Creating ZIP ..."
pushd "$OUTPUT_DIR" >/dev/null
zip -qr "$PACKAGE_ZIP" "$PACKAGE_NAME"
PACKAGE_SHA=$(sha256_file "$PACKAGE_ZIP")
PACKAGE_SIZE=$(get_file_size "$PACKAGE_ZIP")
echo "'$PACKAGE_ZIP' Created! Size: $PACKAGE_SIZE, SHA-256: $PACKAGE_SHA"

if [ "$SKIP_XZ_GENERATION" = true ]; then
    echo "Skipping core XZ (SKIP_XZ_GENERATION=true)"
else
    echo "Creating XZ ..."
    tar -cJf "$PACKAGE_XZ" "$PACKAGE_NAME"
    PACKAGE_XZ_SHA=$(sha256_file "$PACKAGE_XZ")
    PACKAGE_XZ_SIZE=$(get_file_size "$PACKAGE_XZ")
    echo "'$PACKAGE_XZ' Created! Size: $PACKAGE_XZ_SIZE, SHA-256: $PACKAGE_XZ_SHA"
fi
popd >/dev/null

rm -rf "$PKG_DIR"

libs_url=$(jq -r '.packages[0].tools[] | select(.name == "esp32-arduino-libs") | .systems[0].url' "$PACKAGE_JSON_TEMPLATE")
libs_sha=$(jq -r '.packages[0].tools[] | select(.name == "esp32-arduino-libs") | .systems[0].checksum' "$PACKAGE_JSON_TEMPLATE" | sed 's/^SHA-256://')
libs_size=$(jq -r '.packages[0].tools[] | select(.name == "esp32-arduino-libs") | .systems[0].size' "$PACKAGE_JSON_TEMPLATE")

echo "Downloading libs from lib-builder ..."
curl -L -o "$OUTPUT_DIR/$LIBS_ZIP" "$libs_url"

zip_sha=$(sha256_file "$OUTPUT_DIR/$LIBS_ZIP")
zip_size=$(file_size_bytes "$OUTPUT_DIR/$LIBS_ZIP")
if [ "$zip_sha" != "$libs_sha" ] || [ "$zip_size" != "$libs_size" ]; then
    echo "ERROR: ZIP SHA and Size do not match"
    exit 1
fi

echo "Extracting libs for per-SoC packaging ..."
unzip -q "$OUTPUT_DIR/$LIBS_ZIP" -d "$OUTPUT_DIR"
if [ "$SKIP_XZ_GENERATION" = true ]; then
    echo "Skipping libs XZ (SKIP_XZ_GENERATION=true)"
else
    echo "Repacking libs to XZ ..."
    pushd "$OUTPUT_DIR" >/dev/null
    tar -cJf "$LIBS_XZ" "esp32-arduino-libs"
    LIBS_XZ_SHA=$(sha256_file "$LIBS_XZ")
    LIBS_XZ_SIZE=$(get_file_size "$LIBS_XZ")
    popd >/dev/null
fi

mkdir -p "$GITHUB_WORKSPACE/hosted"
cp "$OUTPUT_DIR/esp32-arduino-libs/hosted"/*.bin "$GITHUB_WORKSPACE/hosted/"

echo "Creating per-SoC libs ZIPs..."
SOC_MANIFEST_JSON='[]'
pushd "$OUTPUT_DIR" >/dev/null

for soc_variant in "${CORE_VARIANTS[@]}"; do
    echo "Creating ZIP for $soc_variant..."
    temp_dir="$soc_variant-libs"
    mkdir -p "$temp_dir"

    if [ -d "esp32-arduino-libs/$soc_variant" ]; then
        for item in "esp32-arduino-libs/$soc_variant"/*; do
            name=$(basename "$item")
            if [[ "$name" == "dependencies.lock" || "$name" == "pioarduino-build.py" ]]; then
                continue
            fi
            cp -r "$item" "$temp_dir/"
        done
    else
        echo "ERROR: Variant folder not found: esp32-arduino-libs/$soc_variant"
        exit 1
    fi

    soc_zip="$soc_variant-libs-$RELEASE_TAG.zip"
    zip -qr "$soc_zip" "$temp_dir"
    checksum=$(sha256_file "$soc_zip")
    size=$(file_size_bytes "$soc_zip")
    rm -rf "$temp_dir"

    entry=$(jq -n \
        --arg soc "$soc_variant" \
        --arg file "$soc_zip" \
        --arg sha "$checksum" \
        --argjson size "$size" \
        '{soc: $soc, filename: $file, sha256: $sha, size: $size}')
    SOC_MANIFEST_JSON=$(echo "$SOC_MANIFEST_JSON" | jq --argjson e "$entry" '. + [$e]')
done

popd >/dev/null

rm -rf "${OUTPUT_DIR}/esp32-arduino-libs"
rm -f "${OUTPUT_DIR}/$LIBS_ZIP"

# Save intermediate template state for JSON generation (libs structure without URLs)
existing_systems=$(jq '.packages[0].tools[] | select(.name == "esp32-arduino-libs") | .systems' "$PACKAGE_JSON_TEMPLATE")
jq_args="del(.packages[0].tools[] | select(.name == \"esp32-arduino-libs\"))"
jq_args+=" | del(.packages[0].platforms[0].toolsDependencies[] | select(.name == \"esp32-arduino-libs\"))"

for soc_variant in "${CORE_VARIANTS[@]}"; do
    tool_name="${soc_variant}-libs"
    jq_args+=" | .packages[0].tools += [{\"name\": \"$tool_name\", \"version\": \"${RELEASE_TAG}\", \"systems\": $existing_systems}]"
    jq_args+=" | .packages[0].platforms[0].toolsDependencies += [{\"packager\": \"esp32\", \"name\": \"$tool_name\", \"version\": \"${RELEASE_TAG}\"}]"
done

jq "$jq_args" "$PACKAGE_JSON_TEMPLATE" > "$OUTPUT_DIR/package-libs-template.json"

RVTC_VERSION=$(jq -r ".packages[0].platforms[0].toolsDependencies[] | select(.name == \"$RVTC_NAME\") | .version" "$OUTPUT_DIR/package-libs-template.json" | cut -d '_' -f 2)
RVTC_VERSION=$(date -d "$RVTC_VERSION" '+%y%m' 2>/dev/null || date -j -f '%Y%m%d' "$RVTC_VERSION" '+%y%m')
rvtc_jq_arg="\
    (.packages[0].platforms[0].toolsDependencies[] | select(.name==\"$RVTC_NAME\")).version = \"$RVTC_VERSION\" |\
    (.packages[0].platforms[0].toolsDependencies[] | select(.name==\"$RVTC_NAME\")).name = \"$RVTC_NEW_NAME\" |\
    (.packages[0].tools[] | select(.name==\"$RVTC_NAME\")).version = \"$RVTC_VERSION\" |\
    (.packages[0].tools[] | select(.name==\"$RVTC_NAME\")).name = \"$RVTC_NEW_NAME\" |\
    (.packages[0].platforms[0].toolsDependencies[] | select(.name==\"$X32TC_NAME\")).version = \"$RVTC_VERSION\" |\
    (.packages[0].platforms[0].toolsDependencies[] | select(.name==\"$X32TC_NAME\")).name = \"$X32TC_NEW_NAME\" |\
    (.packages[0].tools[] | select(.name==\"$X32TC_NAME\")).version = \"$RVTC_VERSION\" |\
    (.packages[0].tools[] | select(.name==\"$X32TC_NAME\")).name = \"$X32TC_NEW_NAME\""
jq "$rvtc_jq_arg" "$OUTPUT_DIR/package-libs-template.json" > "$OUTPUT_DIR/package-rvfix-template.json"

jq -n \
    --arg tag "$RELEASE_TAG" \
    --arg pre "$RELEASE_PRE" \
    --arg skip_xz "$SKIP_XZ_GENERATION" \
    --arg core_zip "$PACKAGE_ZIP" \
    --arg core_zip_sha "$PACKAGE_SHA" \
    --argjson core_zip_size "$PACKAGE_SIZE" \
    --arg core_xz "$PACKAGE_XZ" \
    --arg core_xz_sha "${PACKAGE_XZ_SHA:-}" \
    --argjson core_xz_size "${PACKAGE_XZ_SIZE:-0}" \
    --arg libs_xz "$LIBS_XZ" \
    --arg libs_xz_sha "${LIBS_XZ_SHA:-}" \
    --argjson libs_xz_size "${LIBS_XZ_SIZE:-0}" \
    --argjson soc_libs "$SOC_MANIFEST_JSON" \
    '{
        release_tag: $tag,
        prerelease: ($pre == "true"),
        skip_xz: ($skip_xz == "true"),
        core: {
            zip: {filename: $core_zip, sha256: $core_zip_sha, size: $core_zip_size},
            xz: (if $skip_xz == "true" then null else {filename: $core_xz, sha256: $core_xz_sha, size: $core_xz_size} end)
        },
        libs_xz: (if $skip_xz == "true" then null else {filename: $libs_xz, sha256: $libs_xz_sha, size: $libs_xz_size} end),
        soc_libs: $soc_libs
    }' > "$OUTPUT_DIR/manifest.json"

echo "Build complete. Manifest: $OUTPUT_DIR/manifest.json"
ls -la "$OUTPUT_DIR"/*.zip 2>/dev/null || true
if [ "$SKIP_XZ_GENERATION" != true ]; then
    ls -la "$OUTPUT_DIR"/*.tar.xz 2>/dev/null || true
fi
