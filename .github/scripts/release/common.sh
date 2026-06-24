#!/bin/bash
# Shared helpers for release build, GitHub API, and package tests.
# shellcheck disable=SC2181

RELEASE_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPTS_DIR="$(cd "$RELEASE_LIB_DIR/.." && pwd)"
PACKAGE_JSON_MERGE="${SCRIPTS_DIR}/merge_packages.py"

function init_release_output_dir {
    if [ -n "${OUTPUT_DIR:-}" ]; then
        :
    elif [ -n "${GITHUB_ACTIONS:-}" ]; then
        local ws="${GITHUB_WORKSPACE:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
        OUTPUT_DIR="$ws/build"
    else
        OUTPUT_DIR="$RELEASE_LIB_DIR/tmp"
    fi
    export OUTPUT_DIR
    mkdir -p "$OUTPUT_DIR"
}

function empty_release_output_dir {
    init_release_output_dir
    echo "Cleaning release output directory: $OUTPUT_DIR"
    if [ -d "$OUTPUT_DIR" ]; then
        find "$OUTPUT_DIR" -mindepth 1 -maxdepth 1 -exec rm -rf {} +
    fi
    mkdir -p "$OUTPUT_DIR"
}

function clean_release_test_staging_files {
    init_release_output_dir
    rm -f "$OUTPUT_DIR"/*.local.json
    rm -f "$OUTPUT_DIR"/*.raw
    rm -f "$OUTPUT_DIR/package-json-work.json"
    find "$OUTPUT_DIR" -maxdepth 1 -type d -name 'esp32-core-*' -exec rm -rf {} + 2>/dev/null || true
}

function normalize_release_tag {
    echo "${1#v}"
}

function version_bump_file_paths {
    local root="${1:?root required}" scripts_dir rel
    scripts_dir="${2:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
    root="$(cd "$root" && pwd)"
    while IFS= read -r rel; do
        [ -n "$rel" ] && echo "$root/$rel"
    done < <(cd "$root" && "$scripts_dir/update-version.sh" --print-files)
}

function snapshot_version_bump_files {
    local root="$1" snap_dir="$2" f rel dir
    root="$(cd "$root" && pwd)"
    mkdir -p "$snap_dir"
    while IFS= read -r f; do
        [ -f "$f" ] || continue
        rel="${f#"$root"/}"
        dir="$snap_dir/$(dirname "$rel")"
        mkdir -p "$dir"
        cp -p "$f" "$snap_dir/$rel"
    done < <(version_bump_file_paths "$root")
}

function restore_version_bump_from_snapshot {
    local root="$1" snap_dir="$2" rel snap
    root="$(cd "$root" && pwd)"
    [ -d "$snap_dir" ] || return 1
    while IFS= read -r f; do
        rel="${f#"$root"/}"
        snap="$snap_dir/$rel"
        [ -f "$snap" ] || continue
        mkdir -p "$root/$(dirname "$rel")"
        cp -p "$snap" "$root/$rel"
    done < <(version_bump_file_paths "$root")
}

function snapshot_hosted_dir {
    local root="$1" snap_dir="$2" hosted
    root="$(cd "$root" && pwd)"
    hosted="$root/hosted"
    local meta="$snap_dir/_hosted_snapshot"
    mkdir -p "$meta"
    if [ -d "$hosted" ]; then
        echo "present" >"$meta/state"
        if [ -n "$(ls -A "$hosted" 2>/dev/null)" ]; then
            mkdir -p "$meta/files"
            cp -p "$hosted"/* "$meta/files/" 2>/dev/null || true
        fi
    else
        echo "absent" >"$meta/state"
    fi
}

function restore_hosted_from_snapshot {
    local root="$1" snap_dir="$2" hosted
    root="$(cd "$root" && pwd)"
    hosted="$root/hosted"
    local meta="$snap_dir/_hosted_snapshot"
    [ -f "$meta/state" ] || return 0
    local state
    state=$(<"$meta/state")
    if [ "$state" = "absent" ]; then
        rm -rf "$hosted"
        echo "Removed generated hosted/ directory"
        return 0
    fi
    rm -rf "$hosted"
    if [ -d "$meta/files" ] && [ -n "$(ls -A "$meta/files" 2>/dev/null)" ]; then
        mkdir -p "$hosted"
        cp -p "$meta/files"/* "$hosted/"
        echo "Restored hosted/ from snapshot"
    fi
}

function commit_version_bump_only {
    local version="$1" root="${2:-.}" f
    local -a files=()
    version=$(normalize_release_tag "$version")
    root="$(cd "$root" && pwd)"
    while IFS= read -r f; do
        [ -f "$f" ] && files+=("$f")
    done < <(version_bump_file_paths "$root")
    [ "${#files[@]}" -gt 0 ] || return 1
    git -C "$root" add "${files[@]}"
    if git -C "$root" diff --cached --quiet; then
        echo "Version files already at $version"
        git -C "$root" reset HEAD -- "${files[@]}" 2>/dev/null || true
        return 0
    fi
    git -C "$root" commit -m "change(version): Update core version to $version"
}

function git_push_tag_at_ref {
    local tag="$1" ref="${2:-HEAD}"
    tag=$(normalize_release_tag "$tag")
    if [ -n "${GITHUB_ACTIONS:-}" ] || [ -n "${CI:-}" ]; then
        git config user.name "github-actions[bot]"
        git config user.email "41898282+github-actions[bot]@users.noreply.github.com"
    fi
    local resolved
    resolved=$(git rev-parse "$ref^{commit}") || {
        echo "ERROR: cannot resolve git ref: $ref" >&2
        return 1
    }
    git tag -f "$tag" "$resolved"
    git push --force origin "refs/tags/$tag"
    echo "Pushed tag $tag at $(git rev-parse --short "$resolved")"
}

function git_delete_remote_tag {
    local tag="$1"
    tag=$(normalize_release_tag "$tag")
    git push --delete origin "refs/tags/$tag" 2>/dev/null || true
    echo "Deleted remote tag $tag (if it existed)"
}

function commit_version_and_tag {
    local version="${1:?version required}"
    local scripts_dir="${2:?scripts_dir required}"
    version=$(normalize_release_tag "$version")

    echo "Committing version bump and tagging $version ..."
    "${scripts_dir}/update-version.sh" "$version"
    if [ -n "${GITHUB_ACTIONS:-}" ] || [ -n "${CI:-}" ]; then
        git config user.name "github-actions[bot]"
        git config user.email "41898282+github-actions[bot]@users.noreply.github.com"
    fi
    git add .
    if git diff --cached --quiet; then
        echo "Version files already at $version"
    else
        git commit -m "change(version): Update core version to $version"
    fi
    git push
    git tag -f "$version" "$(git rev-parse HEAD)"
    git push --force origin "$version"
    echo "Tagged $version at $(git rev-parse --short HEAD)"
}

function next_patch_version {
    local latest
    latest=$(git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//') || return 1
    local major minor patch
    major=$(echo "$latest" | cut -d. -f1)
    minor=$(echo "$latest" | cut -d. -f2)
    patch=$(echo "$latest" | cut -d. -f3 | sed 's/[^0-9].*//')
    echo "${major}.${minor}.$((patch + 1))"
}

function replace_literal_skip_n {
    local skip_n="$1" from_literal="$2" to_literal="$3" infile="$4" outfile="$5"
    SKIP="$skip_n" FROM="$from_literal" TO="$to_literal" \
        perl -pe 'BEGIN{$s=$ENV{SKIP}+0;$from=$ENV{FROM};$to=$ENV{TO};$i=0;} s/\Q$from\E/($i++<$s)?$&:$to/ge' \
        "$infile" > "$outfile"
}

if [ -z "${RELEASE_OUTPUT_DIR_INIT:-}" ]; then
    RELEASE_OUTPUT_DIR_INIT=1
    init_release_output_dir
fi

function merge_package_json {
    local jsonLink=$1 jsonOut=$2
    local output_dir="${OUTPUT_DIR:?}"
    local old_json="$output_dir/oldJson.json"
    local merged_json="$output_dir/mergedJson.json"

    curl -L -o "$old_json" "https://github.com/$GITHUB_REPOSITORY/releases/download/$jsonLink?access_token=$GITHUB_TOKEN" 2>/dev/null
    set +e
    stdbuf -oL python "$PACKAGE_JSON_MERGE" "$jsonOut" "$old_json" > "$merged_json"
    set -e
    if [ ! -s "$merged_json" ]; then
        rm -f "$merged_json"
    else
        rm -f "$jsonOut"
        mv "$merged_json" "$jsonOut"
    fi
    rm -f "$old_json"
}

function sha256_file {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    else
        shasum -a 256 "$1" | cut -f1 -d' '
    fi
}

function file_size_bytes {
    if [[ "$OSTYPE" == "darwin"* ]]; then stat -f%z "$1"; else stat -c%s "$1"; fi
}

function get_file_size {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        eval "$(stat -s "$1")"; echo "${st_size:?}"
    else
        stat --printf="%s" "$1"
    fi
}

function git_upload_asset {
    curl -X POST -sH "Authorization: token $GITHUB_TOKEN" -H "Content-Type: application/octet-stream" \
        --data-binary @"$1" "https://uploads.github.com/repos/$GITHUB_REPOSITORY/releases/$2/assets?name=$(basename "$1")"
}

function git_safe_upload_asset {
    local file="$1" release_id="$2" upload_res size up_size
    size=$(get_file_size "$file")
    upload_res=$(git_upload_asset "$file" "$release_id")
    up_size=$(echo "$upload_res" | jq -r '.size')
    [ "$up_size" -eq "$size" ] || return 1
    echo "$upload_res" | jq -r '.browser_download_url'
}

function git_safe_upload_asset_record {
    local file="$1" release_id="$2" upload_res size up_size
    size=$(get_file_size "$file")
    upload_res=$(git_upload_asset "$file" "$release_id")
    up_size=$(echo "$upload_res" | jq -r '.size')
    [ "$up_size" -eq "$size" ] || return 1
    echo "$upload_res" | jq '{url: .browser_download_url, id: .id}'
}

function draft_asset_public_url {
    local draft_assets="$1" filename="$2"
    jq -r --arg f "$filename" '.assets[$f] | if type == "object" then .url else . end // empty' "$draft_assets"
}

function verify_release_asset_api {
    local asset_id="$1" code
    [ -n "$asset_id" ] && [ "$asset_id" != "null" ] || {
        echo "ERROR: missing asset id for API verification" >&2
        return 1
    }
    [ -n "${GITHUB_TOKEN:-}" ] && [ -n "${GITHUB_REPOSITORY:-}" ] || {
        echo "ERROR: GITHUB_TOKEN and GITHUB_REPOSITORY required" >&2
        return 1
    }
    code=$(curl -s -o /dev/null -w '%{http_code}' \
        -H "Authorization: token $GITHUB_TOKEN" \
        -H "Accept: application/octet-stream" \
        "https://api.github.com/repos/$GITHUB_REPOSITORY/releases/assets/$asset_id" || echo "000")
    case "$code" in
        200|302)
            echo "Verified draft asset via API (HTTP $code), asset id $asset_id"
            return 0
            ;;
    esac
    echo "ERROR: draft asset API download failed (HTTP ${code:-?}), asset id $asset_id" >&2
    return 1
}

function git_upload_to_pages {
    local path=$1 src=$2 info type message sha="" content="" data
    info=$(curl -s -H "Authorization: token $GITHUB_TOKEN" -H "Accept: application/vnd.github.v3.object+json" \
        -X GET "https://api.github.com/repos/$GITHUB_REPOSITORY/contents/$path?ref=gh-pages")
    type=$(echo "$info" | jq -r '.type')
    message=$(basename "$path")
    if [ "$type" == "file" ]; then
        sha=$(echo "$info" | jq -r '.sha'); message="Updating $message"
    fi
    content=$(base64 < "$src")
    data=$(jq -n --arg branch "gh-pages" --arg message "$message" --arg content "$content" \
        '{branch: $branch, message: $message, content: $content}')
    if [ -n "$sha" ]; then
        data=$(echo "$data" | jq --arg sha "$sha" '. + {sha: $sha}')
    fi
    echo "$data" | curl -s -H "Authorization: token $GITHUB_TOKEN" -H "Accept: application/vnd.github.v3.raw+json" \
        -X PUT --data @- "https://api.github.com/repos/$GITHUB_REPOSITORY/contents/$path"
}

function git_safe_upload_to_pages {
    local path=$1 file="$2" upload_res size up_size
    size=$(get_file_size "$file")
    upload_res=$(git_upload_to_pages "$path" "$file")
    up_size=$(echo "$upload_res" | jq -r '.content.size')
    [ "$up_size" -eq "$size" ] || return 1
    echo "$upload_res" | jq -r '.content.download_url'
}

GITHUB_PAGES_BRANCH="${GITHUB_PAGES_BRANCH:-gh-pages}"

function git_pages_authed_url {
    [ -n "${GITHUB_TOKEN:-}" ] && [ -n "${GITHUB_REPOSITORY:-}" ] || {
        echo "ERROR: GITHUB_TOKEN and GITHUB_REPOSITORY required" >&2
        return 1
    }
    echo "https://x-access-token:${GITHUB_TOKEN}@github.com/${GITHUB_REPOSITORY}.git"
}

function git_config_actions_bot {
    if [ -n "${GITHUB_ACTIONS:-}" ] || [ -n "${CI:-}" ]; then
        git config user.name "github-actions[bot]"
        git config user.email "41898282+github-actions[bot]@users.noreply.github.com"
    fi
}

function git_pages_clone {
    local dest="$1"
    rm -rf "$dest"
    git clone --depth 1 --branch "$GITHUB_PAGES_BRANCH" "$(git_pages_authed_url)" "$dest"
}

function git_pages_commit_push {
    local worktree="$1" message="$2"
    (
        cd "$worktree" || exit 1
        git_config_actions_bot
        git add -A
        if git diff --cached --quiet; then
            echo "No gh-pages changes for: $message"
            exit 0
        fi
        git commit -m "$message"
        git push origin "HEAD:$GITHUB_PAGES_BRANCH"
        echo "Pushed gh-pages: $message"
    )
}

function git_delete_pages_file {
    local path=$1 info sha message
    [ -n "${GITHUB_TOKEN:-}" ] && [ -n "${GITHUB_REPOSITORY:-}" ] || {
        echo "ERROR: GITHUB_TOKEN and GITHUB_REPOSITORY required" >&2
        return 1
    }
    info=$(curl -s -H "Authorization: token $GITHUB_TOKEN" -H "Accept: application/vnd.github.v3.object+json" \
        -X GET "https://api.github.com/repos/$GITHUB_REPOSITORY/contents/$path?ref=gh-pages")
    sha=$(echo "$info" | jq -r '.sha')
    [ -n "$sha" ] && [ "$sha" != "null" ] || return 0
    message="Remove release-staging $(basename "$path")"
    curl -s -X DELETE -H "Authorization: token $GITHUB_TOKEN" -H "Accept: application/vnd.github.v3.raw+json" \
        "https://api.github.com/repos/$GITHUB_REPOSITORY/contents/$path" \
        -d "$(jq -n --arg message "$message" --arg sha "$sha" \
            '{message: $message, sha: $sha, branch: "gh-pages"}')"
    echo "Deleted gh-pages/$path"
}

# Remove every file under a gh-pages directory (e.g. release-staging/).
function git_delete_pages_staging_dir {
    local dir="${1:?directory required}" info path remaining
    [ -n "${GITHUB_TOKEN:-}" ] && [ -n "${GITHUB_REPOSITORY:-}" ] || {
        echo "ERROR: GITHUB_TOKEN and GITHUB_REPOSITORY required" >&2
        return 1
    }
    info=$(curl -s -H "Authorization: token $GITHUB_TOKEN" \
        -H "Accept: application/vnd.github.v3.object+json" \
        -X GET "https://api.github.com/repos/$GITHUB_REPOSITORY/contents/$dir?ref=gh-pages")
    while IFS= read -r path; do
        [ -n "$path" ] && git_delete_pages_file "$path"
    done < <(echo "$info" | jq -r '
        if type == "array" then .[].path
        elif .type == "file" then .path
        elif .type == "dir" then .entries[].path
        else empty end')

    remaining=$(curl -s -H "Authorization: token $GITHUB_TOKEN" \
        -X GET "https://api.github.com/repos/$GITHUB_REPOSITORY/contents/$dir?ref=gh-pages")
    if echo "$remaining" | jq -e '
        if type == "array" then length > 0
        elif .type == "file" then true
        elif .type == "dir" then ((.entries // []) | length) > 0
        else false end' >/dev/null; then
        echo "ERROR: gh-pages/$dir/ still has files after cleanup" >&2
        echo "$remaining" | jq -r '.[].path? // .entries[]?.path? // .path?' >&2 || true
        return 1
    fi
    echo "Cleaned gh-pages/$dir/"
}

function git_create_draft_release {
    local tag="$1" target="$2" prerelease="$3" name="${4:-}"
    local payload pre_json
    pre_json=$(if [ "$prerelease" = "true" ]; then echo true; else echo false; fi)
    if [ -z "$name" ]; then
        if [ -n "$tag" ]; then
            name="Release $tag"
        else
            name="Draft release"
        fi
    fi
    if [ -n "$tag" ]; then
        payload=$(jq -n --arg tag "$tag" --arg target "$target" --arg name "$name" --argjson pre "$pre_json" \
            '{tag_name: $tag, target_commitish: $target, name: $name, draft: true, prerelease: $pre, generate_release_notes: false}')
    else
        payload=$(jq -n --arg target "$target" --arg name "$name" --argjson pre "$pre_json" \
            '{target_commitish: $target, name: $name, draft: true, prerelease: $pre, generate_release_notes: false}')
    fi
    curl -s -X POST -H "Authorization: token $GITHUB_TOKEN" -H "Accept: application/vnd.github+json" \
        "https://api.github.com/repos/$GITHUB_REPOSITORY/releases" -d "$payload"
}

function git_publish_release {
    local release_id="$1" tag_name="$2" prerelease="${3:-false}"
    local payload pre_json
    pre_json=$(if [ "$prerelease" = "true" ]; then echo true; else echo false; fi)
    payload=$(jq -n --arg tag "$tag_name" --argjson pre "$pre_json" \
        '{draft: false, tag_name: $tag, prerelease: $pre}')
    curl -s -X PATCH -H "Authorization: token $GITHUB_TOKEN" -H "Accept: application/vnd.github+json" \
        "https://api.github.com/repos/$GITHUB_REPOSITORY/releases/$release_id" -d "$payload"
}

function git_delete_release {
    curl -s -X DELETE -H "Authorization: token $GITHUB_TOKEN" -H "Accept: application/vnd.github+json" \
        "https://api.github.com/repos/$GITHUB_REPOSITORY/releases/$1"
}

function git_find_release_id_by_tag {
    local tag="$1" page=1 count id releases
    tag=$(normalize_release_tag "$tag")
    [ -n "${GITHUB_TOKEN:-}" ] && [ -n "${GITHUB_REPOSITORY:-}" ] || return 1
    while [ "$page" -le 10 ]; do
        releases=$(curl -s -H "Authorization: token $GITHUB_TOKEN" -H "Accept: application/vnd.github+json" \
            "https://api.github.com/repos/$GITHUB_REPOSITORY/releases?per_page=100&page=$page")
        id=$(echo "$releases" | jq -r --arg t "$tag" '.[] | select(.tag_name == $t) | .id' | head -1)
        if [ -n "$id" ] && [ "$id" != "null" ]; then
            echo "$id"
            return 0
        fi
        count=$(echo "$releases" | jq 'length')
        [ "${count:-0}" -lt 100 ] && break
        page=$((page + 1))
    done
    return 1
}

function git_find_draft_release_id_by_name {
    local name="$1" page=1 count id releases
    [ -n "${GITHUB_TOKEN:-}" ] && [ -n "${GITHUB_REPOSITORY:-}" ] || return 1
    while [ "$page" -le 10 ]; do
        releases=$(curl -s -H "Authorization: token $GITHUB_TOKEN" -H "Accept: application/vnd.github+json" \
            "https://api.github.com/repos/$GITHUB_REPOSITORY/releases?per_page=100&page=$page")
        id=$(echo "$releases" | jq -r --arg n "$name" '.[] | select(.draft == true and .name == $n) | .id' | head -1)
        if [ -n "$id" ] && [ "$id" != "null" ]; then
            echo "$id"
            return 0
        fi
        count=$(echo "$releases" | jq 'length')
        [ "${count:-0}" -lt 100 ] && break
        page=$((page + 1))
    done
    return 1
}

function git_get_release_assets {
    curl -s -H "Authorization: token $GITHUB_TOKEN" -H "Accept: application/vnd.github+json" \
        "https://api.github.com/repos/$GITHUB_REPOSITORY/releases/$1/assets"
}

function get_file_url {
    local file_path="$1"
    if [[ "${OS_IS_WINDOWS:-0}" == "1" ]]; then
        echo "file:///$(cygpath -m "$(realpath "$file_path" 2>/dev/null || echo "$file_path")")"
    elif [[ "$file_path" = /* ]]; then
        echo "file://$file_path"
    else
        echo "file://$(cd "$(dirname "$file_path")" && pwd)/$(basename "$file_path")"
    fi
}

function resolve_release_core_version {
    local version json_file repo url ws
    if [ -n "${RELEASE_VERSION:-}" ]; then
        normalize_release_tag "$RELEASE_VERSION"
        return 0
    fi
    if [ -f "${OUTPUT_DIR}/manifest.json" ]; then
        version=$(jq -r '.release_tag // empty' "${OUTPUT_DIR}/manifest.json")
        if [ -n "$version" ] && [ "$version" != "null" ]; then
            echo "$version"
            return 0
        fi
    fi
    for json_file in \
        "${OUTPUT_DIR}/package_esp32_dev_index.json" \
        "${OUTPUT_DIR}/package_esp32_index.json"; do
        if [ -f "$json_file" ]; then
            version=$(jq -r '.packages[0].platforms[0].version // empty' "$json_file")
            if [ -n "$version" ] && [ "$version" != "null" ]; then
                echo "$version"
                return 0
            fi
        fi
    done
    ws="${GITHUB_WORKSPACE:-}"
    if [ -n "$ws" ] && [ -f "$ws/platform.txt" ]; then
        version=$(grep -m1 '^version=' "$ws/platform.txt" | cut -d= -f2- | tr -d '\r')
        if [ -n "$version" ]; then
            echo "$version"
            return 0
        fi
    fi
    if [ "${RELEASE_TEST_MODE:-}" = "post" ]; then
        repo="${GITHUB_REPOSITORY:-espressif/arduino-esp32}"
        for url in \
            "https://raw.githubusercontent.com/${repo}/gh-pages/release-staging/package_esp32_dev_index.json" \
            "https://raw.githubusercontent.com/${repo}/gh-pages/package_esp32_dev_index.json"; do
            version=$(curl -fsSL "$url" 2>/dev/null | jq -r '.packages[0].platforms[0].version // empty' || true)
            if [ -n "$version" ] && [ "$version" != "null" ]; then
                echo "$version"
                return 0
            fi
        done
    fi
    echo "ERROR: could not determine expected core version (run build-packages first)" >&2
    return 1
}

function installed_esp32_core_version {
    local hw_dir installed expected
    expected="${EXPECTED_CORE_VERSION:-}"
    hw_dir="$(arduino_packages_dir)/packages/esp32/hardware/esp32"
    [ -d "$hw_dir" ] || return 1
    if [ -n "$expected" ] && [ -d "$hw_dir/$expected" ]; then
        echo "$expected"
        return 0
    fi
    installed=$(find "$hw_dir" -mindepth 1 -maxdepth 1 -type d -exec basename {} \; 2>/dev/null | sort -V | tail -1)
    [ -n "$installed" ] || return 1
    echo "$installed"
}

function verify_installed_version {
    local installed expected="${EXPECTED_CORE_VERSION:?EXPECTED_CORE_VERSION required — call resolve_release_core_version first}"
    installed=$(arduino-cli core list 2>/dev/null | awk '/^esp32:esp32[[:space:]]/{print $2}')
    if [ -z "$installed" ]; then
        installed=$(installed_esp32_core_version 2>/dev/null || true)
    fi
    if [ -z "$installed" ]; then
        echo "ERROR: esp32:esp32 not installed (expected $expected)" >&2
        return 1
    fi
    if [ "$installed" != "$expected" ]; then
        echo "ERROR: version mismatch (expected $expected, got $installed)" >&2
        return 1
    fi
    echo "Installed version OK: $installed"
}

function arduino_packages_dir {
    if [[ "${OS_IS_MACOS:-0}" == "1" ]]; then
        echo "$HOME/Library/Arduino15"
    elif [[ "${OS_IS_WINDOWS:-0}" == "1" ]]; then
        echo "${LOCALAPPDATA:-$HOME/AppData/Local}/Arduino15"
    else
        echo "$HOME/.arduino15"
    fi
}

function arduino_cli_host {
    # Match package/package_esp32_index.template.json and tools/get.py identify_platform().
    if [[ "${OS_IS_MACOS:-0}" == "1" ]]; then
        case "$(uname -m)" in
            arm64) echo "arm64-apple-darwin" ;;
            *) echo "x86_64-apple-darwin" ;;
        esac
    elif [[ "${OS_IS_WINDOWS:-0}" == "1" ]]; then
        case "$(uname -m 2>/dev/null)" in
            i686|i386) echo "i686-mingw32" ;;
            *) echo "x86_64-mingw32" ;;
        esac
    else
        case "$(uname -m)" in
            aarch64|arm64) echo "aarch64-linux-gnu" ;;
            i686|i386) echo "i686-pc-linux-gnu" ;;
            *) echo "x86_64-pc-linux-gnu" ;;
        esac
    fi
}

function esp32_toolchain_hosts_equivalent {
    local expected="$1" actual="$2"
    [ -n "$actual" ] || return 1
    [ "$expected" = "$actual" ] && return 0
    case "$expected" in
        x86_64-pc-linux-gnu)
            [[ "$actual" == "x86_64-linux-gnu" ]] && return 0
            ;;
        x86_64-linux-gnu)
            [[ "$actual" == "x86_64-pc-linux-gnu" ]] && return 0
            ;;
        i686-pc-linux-gnu)
            [[ "$actual" == "i686-linux-gnu" ]] && return 0
            ;;
        aarch64-linux-gnu)
            [[ "$actual" == "arm64-linux-gnu" ]] && return 0
            ;;
        arm64-linux-gnu)
            [[ "$actual" == "aarch64-linux-gnu" ]] && return 0
            ;;
        x86_64-mingw32)
            [[ "$actual" == "x86_64-w64-mingw32" || "$actual" == "x86_64-pc-windows-gnu" ]] && return 0
            ;;
        i686-mingw32)
            [[ "$actual" == "i686-w64-mingw32" ]] && return 0
            ;;
    esac
    return 1
}

function esp32_toolchain_host_matches {
    local pkg_json="$1"
    local expected host system
    expected="$(arduino_cli_host)"
    [ -f "$pkg_json" ] || return 1
    host=$(python3 -c "import json; print(json.load(open('$pkg_json')).get('host', ''))" 2>/dev/null || true)
    system=$(python3 -c "import json; print(json.load(open('$pkg_json')).get('system', ''))" 2>/dev/null || true)
    case "$expected" in
        arm64-apple-darwin)
            [[ "$host" == "arm64-apple-darwin" || "$host" == "aarch64-apple-darwin" || "$system" == "darwin_arm64" ]] && return 0
            ;;
        x86_64-apple-darwin)
            [[ "$host" == "x86_64-apple-darwin" || "$system" == "darwin_x86_64" ]] && return 0
            ;;
        *)
            esp32_toolchain_hosts_equivalent "$expected" "$host" && return 0
            esp32_toolchain_hosts_equivalent "$expected" "$system" && return 0
            ;;
    esac
    return 1
}

function esp32_toolchain_gpp {
    local tool="$1"
    local tools_dir
    tools_dir="$(arduino_packages_dir)/packages/esp32/tools"
    if [[ "${OS_IS_WINDOWS:-0}" == "1" ]]; then
        find "$tools_dir/$tool" -name '*g++*' -type f 2>/dev/null | head -1
    else
        find "$tools_dir/$tool" -name '*g++' -type f -perm -111 2>/dev/null | head -1
    fi
}

function esp32_toolchain_usable {
    local tool="$1" gpp
    gpp=$(esp32_toolchain_gpp "$tool")
    [ -n "$gpp" ] && [ -x "$gpp" ]
}

function purge_stale_esp32_toolchains {
    local tools_dir tool reason removed=""
    tools_dir="$(arduino_packages_dir)/packages/esp32/tools"
    for tool in esp-x32 esp-rv32; do
        [ -d "$tools_dir/$tool" ] || continue
        if esp32_toolchain_usable "$tool"; then
            continue
        fi
        reason="incomplete"
        removed="${removed:+$removed, }$tool ($reason)"
        rm -rf "$tools_dir/$tool"
    done
    if [ -n "$removed" ]; then
        echo "Cleaned stale esp32 toolchains: $removed"
    fi
}

function verify_core_toolchain {
    local gpp
    gpp=$(esp32_toolchain_gpp esp-x32)
    if [ -n "$gpp" ] && [ -x "$gpp" ]; then
        echo "Toolchain OK: $gpp"
        return 0
    fi
    echo "ERROR: esp-x32 toolchain incomplete (no g++ under $(arduino_packages_dir)/packages/esp32/tools/esp-x32)" >&2
    return 1
}

function ide_v1_expected_toolchain_host {
    # Arduino IDE 1.8.x macOS bundle is x86_64; on Apple Silicon it runs under Rosetta.
    if [[ "${OS_IS_MACOS:-0}" == "1" ]] && [[ "$(uname -m)" == "arm64" ]]; then
        echo "x86_64-apple-darwin"
    else
        arduino_cli_host
    fi
}

function ide_v1_toolchain_host_matches {
    local pkg_json="$1" expected host system
    expected="$(ide_v1_expected_toolchain_host)"
    [ -f "$pkg_json" ] || return 1
    host=$(python3 -c "import json; print(json.load(open('$pkg_json')).get('host', ''))" 2>/dev/null || true)
    system=$(python3 -c "import json; print(json.load(open('$pkg_json')).get('system', ''))" 2>/dev/null || true)
    case "$expected" in
        arm64-apple-darwin)
            [[ "$host" == "arm64-apple-darwin" || "$host" == "aarch64-apple-darwin" || "$system" == "darwin_arm64" ]] && return 0
            ;;
        x86_64-apple-darwin)
            [[ "$host" == "x86_64-apple-darwin" || "$system" == "darwin_x86_64" ]] && return 0
            ;;
        *)
            esp32_toolchain_hosts_equivalent "$expected" "$host" && return 0
            esp32_toolchain_hosts_equivalent "$expected" "$system" && return 0
            ;;
    esac
    return 1
}

function ide_v1_toolchain_ready {
    esp32_toolchain_usable esp-x32
}

function prepare_ide_v1_package_test {
    local tools_dir tool pkg_json
    # arduino-cli tests may leave wrong-arch toolchains; IDE v1 needs its own arch.
    purge_stale_esp32_toolchains
    if [[ "${OS_IS_MACOS:-0}" != "1" ]] || [[ "$(uname -m)" != "arm64" ]]; then
        return 0
    fi
    tools_dir="$(arduino_packages_dir)/packages/esp32/tools"
    for tool in esp-x32 esp-rv32; do
        [ -d "$tools_dir/$tool" ] || continue
        if ide_v1_toolchain_host_matches "$(find "$tools_dir/$tool" -mindepth 2 -maxdepth 2 -name package.json 2>/dev/null | head -1)"; then
            continue
        fi
        if esp32_toolchain_usable "$tool"; then
            continue
        fi
        echo "IDE v1 prep: removing $tool (wrong arch for IDE v1 under Rosetta)"
        rm -rf "$tools_dir/$tool"
    done
    if [ -d "$(arduino_packages_dir)/packages/esp32/hardware/esp32" ] && ! ide_v1_toolchain_ready; then
        echo "IDE v1 prep: removing esp32 package (platform without IDE v1 toolchains)"
        rm -rf "$(arduino_packages_dir)/packages/esp32"
    fi
}

function install_esp32_core_for_test {
    local url="$1"
    local spec="esp32:esp32@${EXPECTED_CORE_VERSION:?EXPECTED_CORE_VERSION required — call resolve_release_core_version first}"

    purge_stale_esp32_toolchains
    arduino-cli core uninstall esp32:esp32 2>/dev/null || true
    arduino-cli core install "$spec" --additional-urls "$url"
    if verify_core_toolchain; then
        return 0
    fi

    echo "Forcing toolchain reinstall ..."
    purge_stale_esp32_toolchains
    arduino-cli core uninstall esp32:esp32 2>/dev/null || true
    arduino-cli core install "$spec" --additional-urls "$url"
    verify_core_toolchain
}

function start_local_package_server {
    local out_dir="$1"
    out_dir="$(cd "$out_dir" && pwd)"
    [ -d "$out_dir" ] || { echo "ERROR: package output directory not found: $out_dir" >&2; return 1; }

    local port_file
    port_file="$(mktemp)"
    python3 - "$out_dir" "$port_file" "${LOCAL_PACKAGE_SERVER_PORT:-0}" <<'PY' &
import http.server, socketserver, sys, os
out_dir, port_file, port_arg = sys.argv[1:4]
port = int(port_arg)
class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=out_dir, **kwargs)
    def log_message(self, *args): pass
bind = ("127.0.0.1", port if port else 0)
with socketserver.TCPServer(bind, Handler) as httpd:
    with open(port_file, "w") as f:
        f.write(str(httpd.server_address[1]))
    httpd.serve_forever()
PY
    LOCAL_PACKAGE_SERVER_PID=$!
    export LOCAL_PACKAGE_SERVER_PID

    local port="" i
    for i in $(seq 1 30); do
        if [ -s "$port_file" ]; then
            port="$(<"$port_file")"
            break
        fi
        if ! kill -0 "$LOCAL_PACKAGE_SERVER_PID" 2>/dev/null; then
            wait "$LOCAL_PACKAGE_SERVER_PID" 2>/dev/null || true
            rm -f "$port_file"
            echo "ERROR: local package server exited before binding a port" >&2
            return 1
        fi
        sleep 0.1
    done
    rm -f "$port_file"
    [ -n "$port" ] || { echo "ERROR: local package server failed to bind a port" >&2; return 1; }

    LOCAL_PACKAGE_SERVER_URL="http://127.0.0.1:${port}"
    export LOCAL_PACKAGE_SERVER_URL

    for i in $(seq 1 30); do
        if python3 -c "import urllib.request; urllib.request.urlopen('$LOCAL_PACKAGE_SERVER_URL/', timeout=2)" >/dev/null 2>&1; then
            return 0
        fi
        if ! kill -0 "$LOCAL_PACKAGE_SERVER_PID" 2>/dev/null; then
            echo "ERROR: local package server exited during startup (port $port)" >&2
            return 1
        fi
        sleep 0.1
    done
    echo "ERROR: local package server not reachable at $LOCAL_PACKAGE_SERVER_URL" >&2
    return 1
}

function stop_local_package_server {
    if [ -n "${GITHUB_RELEASE_PROXY_PID:-}" ]; then
        kill "$GITHUB_RELEASE_PROXY_PID" 2>/dev/null || true
        wait "$GITHUB_RELEASE_PROXY_PID" 2>/dev/null || true
        unset GITHUB_RELEASE_PROXY_PID GITHUB_RELEASE_PROXY_URL
    fi
    if [ -n "${LOCAL_PACKAGE_SERVER_PID:-}" ]; then
        kill "$LOCAL_PACKAGE_SERVER_PID" 2>/dev/null || true
        wait "$LOCAL_PACKAGE_SERVER_PID" 2>/dev/null || true
        unset LOCAL_PACKAGE_SERVER_PID LOCAL_PACKAGE_SERVER_URL
    fi
}

function start_github_release_proxy {
    local out_dir="$1" draft_assets="$2"
    local proxy_script="${RELEASE_DIR}/github_release_proxy.py" port_file port="" i

    out_dir="$(cd "$out_dir" && pwd)"
    draft_assets="$(cd "$(dirname "$draft_assets")" && pwd)/$(basename "$draft_assets")"
    [ -d "$out_dir" ] || { echo "ERROR: package output directory not found: $out_dir" >&2; return 1; }
    [ -f "$draft_assets" ] || { echo "ERROR: draft-assets.json not found: $draft_assets" >&2; return 1; }
    [ -n "${GITHUB_TOKEN:-}" ] || { echo "ERROR: GITHUB_TOKEN required for release proxy" >&2; return 1; }
    [ -n "${GITHUB_REPOSITORY:-}" ] || { echo "ERROR: GITHUB_REPOSITORY required for release proxy" >&2; return 1; }

    port_file="$(mktemp)"
    GITHUB_TOKEN="$GITHUB_TOKEN" python3 "$proxy_script" \
        --dir "$out_dir" \
        --assets "$draft_assets" \
        --repo "$GITHUB_REPOSITORY" \
        --port "${GITHUB_RELEASE_PROXY_PORT:-0}" \
        --pid-file "$port_file" &
    GITHUB_RELEASE_PROXY_PID=$!
    export GITHUB_RELEASE_PROXY_PID

    for i in $(seq 1 30); do
        if [ -s "$port_file" ]; then
            port="$(<"$port_file")"
            break
        fi
        if ! kill -0 "$GITHUB_RELEASE_PROXY_PID" 2>/dev/null; then
            wait "$GITHUB_RELEASE_PROXY_PID" 2>/dev/null || true
            rm -f "$port_file"
            echo "ERROR: GitHub release proxy exited before binding a port" >&2
            return 1
        fi
        sleep 0.1
    done
    rm -f "$port_file"
    [ -n "$port" ] || { echo "ERROR: GitHub release proxy failed to bind a port" >&2; return 1; }

    GITHUB_RELEASE_PROXY_URL="http://127.0.0.1:${port}"
    LOCAL_PACKAGE_SERVER_URL="$GITHUB_RELEASE_PROXY_URL"
    export GITHUB_RELEASE_PROXY_URL LOCAL_PACKAGE_SERVER_URL

    for i in $(seq 1 30); do
        if python3 -c "import urllib.request; urllib.request.urlopen('$GITHUB_RELEASE_PROXY_URL/health', timeout=2)" >/dev/null 2>&1; then
            echo "GitHub release proxy: $GITHUB_RELEASE_PROXY_URL"
            return 0
        fi
        if ! kill -0 "$GITHUB_RELEASE_PROXY_PID" 2>/dev/null; then
            echo "ERROR: GitHub release proxy exited during startup (port $port)" >&2
            return 1
        fi
        sleep 0.1
    done
    echo "ERROR: GitHub release proxy not reachable at $GITHUB_RELEASE_PROXY_URL" >&2
    return 1
}

function verify_draft_release_proxy_smoke {
    local draft_assets="$1" sample="" asset_id
    local base="${GITHUB_RELEASE_PROXY_URL:?GITHUB_RELEASE_PROXY_URL required}"

    [ -f "$draft_assets" ] || { echo "ERROR: draft-assets.json not found: $draft_assets" >&2; return 1; }

    sample=$(jq -r '.assets | keys[]' "$draft_assets" | grep -E 'esp32-core-.*\.zip$' | head -1)
    [ -n "$sample" ] || sample=$(jq -r '.assets | keys[0]' "$draft_assets")
    [ -n "$sample" ] || { echo "ERROR: no draft release assets to probe" >&2; return 1; }

    asset_id=$(jq -r --arg f "$sample" '.assets[$f].id' "$draft_assets")
    [ -n "$asset_id" ] && [ "$asset_id" != "null" ] || {
        echo "ERROR: missing asset id for $sample in draft-assets.json" >&2
        return 1
    }

    local headers rc=0
    headers=$(curl -fsSI "${base}/${sample}" 2>/dev/null) || rc=$?
    if [ "$rc" -ne 0 ]; then
        echo "ERROR: draft release proxy probe failed for ${base}/${sample}" >&2
        return 1
    fi
    if ! grep -qi 'X-Draft-Release-Source: github-api' <<<"$headers"; then
        echo "ERROR: proxy did not report GitHub API source for $sample" >&2
        return 1
    fi
    echo "Draft release proxy OK: $sample (asset id $asset_id via GitHub API)"
}

function verify_pre_test_proxy_json {
    local json_file="$1" draft_assets="$2" proxy_base="$3"
    [ -f "$json_file" ] || { echo "ERROR: proxy JSON not found: $json_file" >&2; return 1; }
    [ -f "$draft_assets" ] || { echo "ERROR: draft-assets.json not found: $draft_assets" >&2; return 1; }
    python3 - "$json_file" "$draft_assets" "$proxy_base" <<'PY'
import json, sys
json_file, draft_assets, proxy_base = sys.argv[1:4]
proxy_base = proxy_base.rstrip("/")
with open(draft_assets, encoding="utf-8") as fh:
    draft_names = set((json.load(fh).get("assets") or {}).keys())
with open(json_file, encoding="utf-8") as fh:
    data = json.load(fh)

by_file: dict[str, set[str]] = {}

def note(filename, url):
    if filename and url:
        by_file.setdefault(filename, set()).add(url)

for plat in data.get("packages", [{}])[0].get("platforms", []):
    note(plat.get("archiveFileName", ""), plat.get("url", ""))

for tool in data.get("packages", [{}])[0].get("tools", []):
    for s in tool.get("systems", []):
        note(s.get("archiveFileName", ""), s.get("url", ""))

errors = []
rewritten = 0
for filename, urls in sorted(by_file.items()):
    if filename not in draft_names:
        continue
    expected = f"{proxy_base}/{filename}"
    if expected in urls:
        rewritten += 1
        continue
    sample = sorted(urls)[0]
    errors.append(f"{filename}: expected {expected}, got {sample!r}")

if errors:
    raise SystemExit("ERROR: pre-test proxy JSON invalid:\n  " + "\n  ".join(errors))
if rewritten == 0:
    raise SystemExit("ERROR: no draft release URLs rewritten to proxy in " + json_file)
print(f"Pre-test proxy JSON OK: {rewritten} draft archive URL(s) -> {proxy_base}/")
PY
}

function assert_gh_pages_package_index_url {
    local url="$1"
    case "$url" in
        https://raw.githubusercontent.com/*/gh-pages/package_esp32_*.json) return 0 ;;
        https://raw.githubusercontent.com/*/gh-pages/release-staging/package_esp32_*.json) return 0 ;;
    esac
    echo "ERROR: post-test must use live gh-pages JSON (got $url)" >&2
    return 1
}

function verify_gh_pages_package_json {
    local url="$1" expected_version="$2"
    local remote version platform_url tag

    assert_gh_pages_package_index_url "$url" || return 1

    remote=$(curl -fsSL "$url") || {
        echo "ERROR: failed to fetch gh-pages JSON: $url" >&2
        return 1
    }

    version=$(jq -r '.packages[0].platforms[0].version // empty' <<<"$remote")
    platform_url=$(jq -r '.packages[0].platforms[0].url // empty' <<<"$remote")

    if [ -z "$version" ] || [ -z "$platform_url" ]; then
        echo "ERROR: gh-pages JSON missing platform version/url ($url)" >&2
        return 1
    fi
    if [ "$version" != "$expected_version" ]; then
        echo "ERROR: gh-pages JSON version $version != expected $expected_version" >&2
        return 1
    fi

    case "$platform_url" in
        http://127.0.0.1:*|http://localhost:*|file://*)
            echo "ERROR: gh-pages JSON points at local/proxy URL: $platform_url" >&2
            return 1
            ;;
        https://github.com/*/releases/download/untagged-*)
            echo "ERROR: gh-pages JSON still has draft untagged URLs: $platform_url" >&2
            return 1
            ;;
        https://github.com/*/releases/download/*)
            tag=$(basename "$(dirname "$platform_url")")
            ;;
        *)
            echo "ERROR: gh-pages platform URL is not a published release asset: $platform_url" >&2
            return 1
            ;;
    esac

    echo "gh-pages JSON OK: version=$version tag=$tag url=$platform_url"
}

function rewrite_json_to_proxy {
    local src="$1" dst="$2" draft_assets="$3"
    local base_url="${GITHUB_RELEASE_PROXY_URL:?GITHUB_RELEASE_PROXY_URL required — start GitHub release proxy first}"
    [ -f "$draft_assets" ] || { echo "ERROR: draft-assets.json not found: $draft_assets" >&2; return 1; }
    python3 - "$src" "$dst" "$base_url" "$draft_assets" <<'PY'
import json, sys
src, dst, base_url, draft_assets = sys.argv[1:5]
base_url = base_url.rstrip('/')
with open(draft_assets) as f:
    proxied = set((json.load(f).get('assets') or {}).keys())
if not proxied:
    raise SystemExit(f"ERROR: no draft release assets in {draft_assets}")
def archive_url(filename):
    return f"{base_url}/{filename}"
with open(src) as f:
    data = json.load(f)
def fix_systems(systems):
    for s in systems:
        fn = s.get('archiveFileName', '')
        if fn and fn in proxied:
            s['url'] = archive_url(fn)
    return systems
for pkg in data.get('packages', []):
    for plat in pkg.get('platforms', []):
        fn = plat.get('archiveFileName', '')
        if fn and fn in proxied:
            plat['url'] = archive_url(fn)
    for tool in pkg.get('tools', []):
        tool['systems'] = fix_systems(tool.get('systems', []))
with open(dst, 'w') as f:
    json.dump(data, f, indent=2)
PY
}

function rewrite_json_to_local {
    local src="$1" dst="$2" out_dir="$3"
    local base_url="${LOCAL_PACKAGE_SERVER_URL:?LOCAL_PACKAGE_SERVER_URL required — start local package server first}"
    python3 - "$src" "$dst" "$out_dir" "$base_url" <<'PY'
import json, sys, os
src, dst, out_dir, base_url = sys.argv[1:5]
base_url = base_url.rstrip('/')
def archive_url(filename):
    return f"{base_url}/{filename}"
with open(src) as f:
    data = json.load(f)
def fix_systems(systems):
    for s in systems:
        fn = s.get('archiveFileName', '')
        local = os.path.join(out_dir, fn)
        if fn and os.path.isfile(local):
            s['url'] = archive_url(fn)
    return systems
for pkg in data.get('packages', []):
    for plat in pkg.get('platforms', []):
        fn = plat.get('archiveFileName', '')
        local = os.path.join(out_dir, fn)
        if fn and os.path.isfile(local):
            plat['url'] = archive_url(fn)
    for tool in pkg.get('tools', []):
        tool['systems'] = fix_systems(tool.get('systems', []))
with open(dst, 'w') as f:
    json.dump(data, f, indent=2)
PY
}


function ide_v1_install_boards {
    local url="$1" version_suffix="$2" log rc attempt
    for attempt in 1 2; do
        if [ "$attempt" -eq 2 ]; then
            echo "IDE v1: retrying board install after removing incomplete package ..."
            rm -rf "$(arduino_packages_dir)/packages/esp32"
        fi
        log="$(mktemp)"
        echo "IDE v1: installing esp32:esp32${version_suffix} from package index URL:"
        echo "  $url"
        echo "IDE v1: board manager install (may take several minutes) ..."
        if _arduino_headless_log_filter_enabled; then
            set -o pipefail
            run_with_timeout 1800 run_arduino_ide_v1 \
                --pref "boardsmanager.additional.urls=$url" \
                --install-boards "esp32:esp32${version_suffix}" 2>&1 \
                | _filter_arduino_headless_log_noise | tee "$log"
            rc=${PIPESTATUS[0]}
        else
            run_with_timeout 1800 run_arduino_ide_v1 \
                --pref "boardsmanager.additional.urls=$url" \
                --install-boards "esp32:esp32${version_suffix}" 2>&1 | tee "$log"
            rc=${PIPESTATUS[0]}
        fi
        if grep -aq "Platform is already installed" "$log"; then
            if verify_installed_version && ide_v1_toolchain_ready; then
                rm -f "$log"
                return 0
            fi
            echo "IDE v1: platform already installed but wrong version or incomplete toolchains"
            rm -f "$log"
            continue
        fi
        if [ "$rc" -ne 0 ]; then
            echo "ERROR: IDE v1 board install failed (exit $rc)" >&2
            rm -f "$log"
            return 1
        fi
        if grep -aqiE '^Error:|Exception in thread|java\.lang\.' "$log"; then
            echo "ERROR: IDE v1 board install log contains errors" >&2
            rm -f "$log"
            return 1
        fi
        rm -f "$log"
        if verify_installed_version && ide_v1_toolchain_ready; then
            return 0
        fi
        echo "IDE v1: install finished but version or toolchains are wrong/incomplete"
    done
    echo "ERROR: IDE v1 toolchains not ready after install" >&2
    return 1
}
source "${SCRIPTS_DIR}/arduino_headless.sh"
