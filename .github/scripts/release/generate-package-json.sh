#!/bin/bash
# Generate package JSON files from manifest and URL sources.
# shellcheck disable=SC2181

set -e

RELEASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPTS_DIR="$(cd "$RELEASE_DIR/.." && pwd)"
GITHUB_WORKSPACE="${GITHUB_WORKSPACE:-$(git rev-parse --show-toplevel)}"

RELEASE_TAG="${RELEASE_TAG:?RELEASE_TAG required}"
RELEASE_TAG=$(echo "$RELEASE_TAG" | sed 's/^v//')
RELEASE_PRE="${RELEASE_PRE:-false}"
DRY_RUN="${DRY_RUN:-false}"
JSON_MODE="${JSON_MODE:-test}"  # test | final

PACKAGE_JSON_MERGE="$SCRIPTS_DIR/merge_packages.py"
PACKAGE_JSON_DEV="package_esp32_dev_index.json"
PACKAGE_JSON_REL="package_esp32_index.json"
PACKAGE_JSON_DEV_CN="package_esp32_dev_index_cn.json"
PACKAGE_JSON_REL_CN="package_esp32_index_cn.json"

source "$RELEASE_DIR/common.sh"
source "$SCRIPTS_DIR/socs_config.sh"

MANIFEST="$OUTPUT_DIR/manifest.json"
TEMPLATE="$OUTPUT_DIR/package-rvfix-template.json"
DRAFT_ASSETS="$OUTPUT_DIR/draft-assets.json"

if [ ! -f "$MANIFEST" ] || [ ! -f "$TEMPLATE" ]; then
    echo "ERROR: manifest.json or package-rvfix-template.json not found. Run build-packages.sh first."
    exit 1
fi

function resolve_url {
    local filename="$1"
    if [ "$DRY_RUN" = "true" ]; then
        local path="$OUTPUT_DIR/$filename"
        if [[ "$OSTYPE" == "darwin"* ]]; then
            echo "file://$path"
        else
            realpath "$path" | sed 's|^|file://|'
        fi
        return
    fi
    if [ -f "$DRAFT_ASSETS" ]; then
        local url
        url=$(jq -r --arg f "$filename" '.assets[$f] | if type == "object" then .url else . end // empty' "$DRAFT_ASSETS")
        if [ -n "$url" ]; then
            echo "$url"
            return
        fi
    fi
    echo "ERROR: No URL for $filename (DRY_RUN=$DRY_RUN, draft-assets missing entry)" >&2
    exit 1
}

PACKAGE_ZIP=$(jq -r '.core.zip.filename' "$MANIFEST")
PACKAGE_SHA=$(jq -r '.core.zip.sha256' "$MANIFEST")
PACKAGE_SIZE=$(jq -r '.core.zip.size' "$MANIFEST")
PACKAGE_URL=$(resolve_url "$PACKAGE_ZIP")

# Update per-SoC tool URLs in template
WORK_TEMPLATE="$OUTPUT_DIR/package-json-work.json"
cp "$TEMPLATE" "$WORK_TEMPLATE"

for soc_variant in "${CORE_VARIANTS[@]}"; do
    tool_name="${soc_variant}-libs"
    soc_file=$(jq -r --arg s "$soc_variant" '.soc_libs[] | select(.soc == $s) | .filename' "$MANIFEST")
    soc_sha=$(jq -r --arg s "$soc_variant" '.soc_libs[] | select(.soc == $s) | .sha256' "$MANIFEST")
    soc_size=$(jq -r --arg s "$soc_variant" '.soc_libs[] | select(.soc == $s) | .size' "$MANIFEST")
    soc_url=$(resolve_url "$soc_file")

    jq --arg name "$tool_name" \
       --arg url "$soc_url" \
       --arg file "$soc_file" \
       --arg checksum "SHA-256:$soc_sha" \
       --arg size "$soc_size" \
       '(.packages[0].tools[] | select(.name == $name) | .systems) |=
        map(.url = $url | .archiveFileName = $file | .checksum = $checksum | .size = ($size|tonumber))' \
       "$WORK_TEMPLATE" > "$WORK_TEMPLATE.tmp" && mv "$WORK_TEMPLATE.tmp" "$WORK_TEMPLATE"
done

jq_arg=".packages[0].platforms[0].version = \"$RELEASE_TAG\" | \
    .packages[0].platforms[0].url = \"$PACKAGE_URL\" |\
    .packages[0].platforms[0].archiveFileName = \"$PACKAGE_ZIP\" |\
    .packages[0].platforms[0].size = \"$PACKAGE_SIZE\" |\
    .packages[0].platforms[0].checksum = \"SHA-256:$PACKAGE_SHA\""

echo "Generating $PACKAGE_JSON_DEV ..."
tmp_pkg_json="$OUTPUT_DIR/${PACKAGE_JSON_DEV}.raw"
jq "$jq_arg" "$WORK_TEMPLATE" > "$tmp_pkg_json"
cp "$tmp_pkg_json" "$OUTPUT_DIR/$PACKAGE_JSON_DEV"
replace_literal_skip_n 1 "github.com/" "dl.espressif.cn/github_assets/" "$tmp_pkg_json" "$OUTPUT_DIR/$PACKAGE_JSON_DEV_CN"
rm -f "$tmp_pkg_json"
python "$SCRIPTS_DIR/release_append_cn.py" "$OUTPUT_DIR/$PACKAGE_JSON_DEV_CN"

if [ "$RELEASE_PRE" = "false" ]; then
    echo "Generating $PACKAGE_JSON_REL ..."
    tmp_pkg_json="$OUTPUT_DIR/${PACKAGE_JSON_REL}.raw"
    jq "$jq_arg" "$WORK_TEMPLATE" > "$tmp_pkg_json"
    cp "$tmp_pkg_json" "$OUTPUT_DIR/$PACKAGE_JSON_REL"
    replace_literal_skip_n 1 "github.com/" "dl.espressif.cn/github_assets/" "$tmp_pkg_json" "$OUTPUT_DIR/$PACKAGE_JSON_REL_CN"
    rm -f "$tmp_pkg_json"
    python "$SCRIPTS_DIR/release_append_cn.py" "$OUTPUT_DIR/$PACKAGE_JSON_REL_CN"
fi

if [ "$JSON_MODE" = "final" ]; then
  if [ -z "${GITHUB_TOKEN:-}" ] || [ -z "${GITHUB_REPOSITORY:-}" ]; then
    echo "ERROR: GITHUB_TOKEN and GITHUB_REPOSITORY required for final JSON merge"
    exit 1
  fi

  echo "Getting previous releases ..."
  releasesJson=$(curl -sH "Authorization: token $GITHUB_TOKEN" "https://api.github.com/repos/$GITHUB_REPOSITORY/releases")

  set +e
  prev_release=$(echo "$releasesJson" | jq -e -r ". | map(select(.draft == false and .prerelease == false)) | sort_by(.published_at | - fromdateiso8601) | .[0].tag_name")
  prev_any_release=$(echo "$releasesJson" | jq -e -r ". | map(select(.draft == false)) | sort_by(.published_at | - fromdateiso8601)  | .[0].tag_name")
  shopt -s nocasematch
  if [[ "$prev_release" == "$RELEASE_TAG" ]]; then
      prev_release=$(echo "$releasesJson" | jq -e -r ". | map(select(.draft == false and .prerelease == false)) | sort_by(.published_at | - fromdateiso8601) | .[1].tag_name")
  fi
  if [[ "$prev_any_release" == "$RELEASE_TAG" ]]; then
      prev_any_release=$(echo "$releasesJson" | jq -e -r ". | map(select(.draft == false)) | sort_by(.published_at | - fromdateiso8601)  | .[1].tag_name")
  fi
  shopt -u nocasematch
  set -e

  if [ -n "$prev_any_release" ] && [ "$prev_any_release" != "null" ]; then
      merge_package_json "$prev_any_release/$PACKAGE_JSON_DEV" "$OUTPUT_DIR/$PACKAGE_JSON_DEV"
      merge_package_json "$prev_any_release/$PACKAGE_JSON_DEV_CN" "$OUTPUT_DIR/$PACKAGE_JSON_DEV_CN"
  fi

  if [ "$RELEASE_PRE" = "false" ]; then
      if [ -n "$prev_release" ] && [ "$prev_release" != "null" ]; then
          merge_package_json "$prev_release/$PACKAGE_JSON_REL" "$OUTPUT_DIR/$PACKAGE_JSON_REL"
          merge_package_json "$prev_release/$PACKAGE_JSON_REL_CN" "$OUTPUT_DIR/$PACKAGE_JSON_REL_CN"
      fi
  fi
fi

echo "Package JSON generation complete ($JSON_MODE mode, DRY_RUN=$DRY_RUN)"
ls -la "$OUTPUT_DIR"/package_esp32_*.json
