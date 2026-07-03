#!/bin/bash
# GitHub release operations: draft | tag | publish | candidate | promote | rollback | delete | delete-resources
# shellcheck disable=SC2181

set -e

RELEASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPTS_DIR="$(cd "$RELEASE_DIR/.." && pwd)"
source "$RELEASE_DIR/common.sh"

GITHUB_WORKSPACE="${GITHUB_WORKSPACE:-$(pwd)}"
ACTION="${1:?Usage: github-release.sh draft|tag|publish|candidate|promote|rollback|delete|delete-resources}"

RELEASE_PRE="${RELEASE_PRE:-false}"
BUILD_REF="${BUILD_REF:-HEAD}"
RELEASE_STAGING_DIR="${RELEASE_STAGING_DIR:-release-staging}"

if [ "$ACTION" != "delete" ] && [ "$ACTION" != "delete-resources" ]; then
    RELEASE_TAG="${RELEASE_TAG:?RELEASE_TAG required}"
    RELEASE_TAG=$(normalize_release_tag "$RELEASE_TAG")
fi

package_json_names() {
    local dev=package_esp32_dev_index.json rel=package_esp32_index.json
    local dev_cn=package_esp32_dev_index_cn.json rel_cn=package_esp32_index_cn.json
    echo "$dev" "$dev_cn"
    if [ "$RELEASE_PRE" = "false" ]; then
        echo "$rel" "$rel_cn"
    fi
}

cmd_draft() {
    MANIFEST="$OUTPUT_DIR/manifest.json"
    [ -f "$MANIFEST" ] || { echo "ERROR: manifest.json not found"; exit 1; }
    [ -n "${GITHUB_TOKEN:-}" ] && [ -n "${GITHUB_REPOSITORY:-}" ] || { echo "ERROR: GITHUB_TOKEN and GITHUB_REPOSITORY required"; exit 1; }

    # GitHub API requires tag_name; no git tag is created until publish (draft shows as untagged).
    local draft_tag="${DRAFT_RELEASE_TAG:-$RELEASE_TAG}"
    local release_name="Release $RELEASE_TAG"
    local release_res RELEASE_ID assets_json='{}' core_fn core_id
    release_res=$(git_create_draft_release "$draft_tag" "$BUILD_REF" "$RELEASE_PRE" "$release_name")
    RELEASE_ID=$(echo "$release_res" | jq -r '.id')
    [ -n "$RELEASE_ID" ] && [ "$RELEASE_ID" != "null" ] || { echo "$release_res"; exit 1; }

    upload_record() {
        local fn="$1" record url asset_id
        [ -n "$fn" ] && [ "$fn" != "null" ] || return 0
        [ -f "$OUTPUT_DIR/$fn" ] || { echo "ERROR: missing $fn"; exit 1; }
        record=$(git_safe_upload_asset_record "$OUTPUT_DIR/$fn" "$RELEASE_ID")
        url=$(echo "$record" | jq -r '.url')
        asset_id=$(echo "$record" | jq -r '.id')
        assets_json=$(echo "$assets_json" | jq --arg k "$fn" --arg url "$url" --argjson id "$asset_id" \
            '. + {($k): {url: $url, id: $id}}')
        echo "Uploaded $fn (asset id $asset_id)"
    }

    upload_record "$(jq -r '.core.zip.filename' "$MANIFEST")"
    upload_record "$(jq -r '.core.xz.filename // empty' "$MANIFEST")"
    upload_record "$(jq -r '.libs_xz.filename // empty' "$MANIFEST")"
    while IFS= read -r soc_file; do upload_record "$soc_file"; done < <(jq -r '.soc_libs[].filename' "$MANIFEST")

    jq -n --argjson id "$RELEASE_ID" --arg tag "$draft_tag" --argjson assets "$assets_json" \
        '{release_id: $id, tag_name: $tag, assets: $assets}' > "$OUTPUT_DIR/draft-assets.json"

    core_fn=$(jq -r '.core.zip.filename' "$MANIFEST")
    core_id=$(jq -r --arg f "$core_fn" '.[$f].id' <<<"$assets_json")
    verify_release_asset_api "$core_id"

    echo "Draft release $RELEASE_ID ready (untagged; pre-release tests use API proxy)"
}

cmd_publish() {
    DRAFT_ASSETS="$OUTPUT_DIR/draft-assets.json"
    [ -f "$DRAFT_ASSETS" ] || { echo "ERROR: draft-assets.json not found"; exit 1; }
    local RELEASE_ID result draft assets assets_map='{}'
    RELEASE_ID=$(jq -r '.release_id' "$DRAFT_ASSETS")
    result=$(git_publish_release "$RELEASE_ID" "$RELEASE_TAG" "$RELEASE_PRE")
    draft=$(echo "$result" | jq -r '.draft')
    [ "$draft" = "false" ] || { echo "$result"; exit 1; }
    assets=$(git_get_release_assets "$RELEASE_ID")
    while IFS= read -r row; do
        assets_map=$(echo "$assets_map" | jq --arg n "$(echo "$row" | jq -r '.name')" \
            --arg u "$(echo "$row" | jq -r '.browser_download_url')" '. + {($n): $u}')
    done < <(echo "$assets" | jq -c '.[]')
    jq --argjson assets "$assets_map" --arg tag "$RELEASE_TAG" \
        '.assets = $assets | .tag_name = $tag' "$DRAFT_ASSETS" > "$DRAFT_ASSETS.tmp"
    mv "$DRAFT_ASSETS.tmp" "$DRAFT_ASSETS"
    echo "Published release $RELEASE_TAG"
}

cmd_tag() {
    [ -n "${GITHUB_TOKEN:-}" ] || { echo "ERROR: GITHUB_TOKEN required"; exit 1; }
    commit_version_and_tag "$RELEASE_TAG" "$SCRIPTS_DIR"
}

cmd_candidate() {
    DRAFT_ASSETS="$OUTPUT_DIR/draft-assets.json"
    RELEASE_ID=$(jq -r '.release_id' "$DRAFT_ASSETS")
    local name staging_path worktree

    [ -f "$OUTPUT_DIR/package_esp32_dev_index.json" ] || {
        echo "ERROR: package_esp32_dev_index.json not found (generate with JSON_MODE=final before candidate)" >&2
        exit 1
    }

    worktree=$(mktemp -d)
    trap 'rm -rf "$worktree"' EXIT
    git_pages_clone "$worktree"

    for name in $(package_json_names); do
        [ -f "$OUTPUT_DIR/$name" ] || { echo "ERROR: missing $OUTPUT_DIR/$name"; exit 1; }
        staging_path="${RELEASE_STAGING_DIR}/${name}"
        git_safe_upload_asset "$OUTPUT_DIR/$name" "$RELEASE_ID"
        mkdir -p "$worktree/$(dirname "$staging_path")"
        cp -p "$OUTPUT_DIR/$name" "$worktree/$staging_path"
        echo "Staged $name on gh-pages/$staging_path"
    done

    git_pages_commit_push "$worktree" "Stage package JSON for release $RELEASE_TAG"
    echo "Candidate upload complete (gh-pages/$RELEASE_STAGING_DIR/)"
}

cmd_promote() {
    local name worktree hosted_src="${HOSTED_ARTIFACT_DIR:-}"
    worktree=$(mktemp -d)
    trap 'rm -rf "$worktree"' EXIT
    git_pages_clone "$worktree"

    for name in $(package_json_names); do
        [ -f "$OUTPUT_DIR/$name" ] || { echo "ERROR: missing $OUTPUT_DIR/$name"; exit 1; }
        cp -p "$OUTPUT_DIR/$name" "$worktree/$name"
        echo "Promoted $name to gh-pages root"
    done
    rm -rf "$worktree/$RELEASE_STAGING_DIR"

    if [ -n "$hosted_src" ] && [ -d "$hosted_src" ]; then
        mkdir -p "$worktree/hosted"
        cp --update=none "$hosted_src"/*.bin "$worktree/hosted/" 2>/dev/null || true
    fi

    git_pages_commit_push "$worktree" "Release $RELEASE_TAG package JSON"
    echo "Promote complete"
}

cmd_rollback() {
    DRAFT_ASSETS="$OUTPUT_DIR/draft-assets.json"
    local RELEASE_ID="${RELEASE_ID:-}" worktree

    if [ -z "$RELEASE_ID" ] || [ "$RELEASE_ID" = "null" ]; then
        RELEASE_ID=$(jq -r '.release_id' "$DRAFT_ASSETS" 2>/dev/null || true)
    fi
    if [ -z "$RELEASE_ID" ] || [ "$RELEASE_ID" = "null" ]; then
        RELEASE_ID=$(git_find_release_id_by_tag "$RELEASE_TAG" 2>/dev/null || true)
    fi

    worktree=$(mktemp -d)
    trap 'rm -rf "$worktree"' EXIT
    git_pages_clone "$worktree"
    rm -rf "$worktree/$RELEASE_STAGING_DIR"
    git_pages_commit_push "$worktree" "Rollback release $RELEASE_TAG staging JSON" || true

    if [ -n "$RELEASE_ID" ] && [ "$RELEASE_ID" != "null" ]; then
        git_delete_release "$RELEASE_ID"
        echo "Deleted release $RELEASE_ID"
    else
        echo "No release id found to delete"
    fi
    git_delete_remote_tag "$RELEASE_TAG"
    echo "Rollback complete"
}

cmd_delete() {
    local id="${RELEASE_ID:-$(jq -r '.release_id' "$OUTPUT_DIR/draft-assets.json" 2>/dev/null)}"
    [ -n "$id" ] && [ "$id" != "null" ] || exit 0
    git_delete_release "$id"
    echo "Deleted draft release $id"
}

cmd_delete_resources() {
    local id="${RELEASE_ID:-}"
    [ -n "${GITHUB_TOKEN:-}" ] || { echo "ERROR: GITHUB_TOKEN required"; exit 1; }

    if [ -z "$id" ] || [ "$id" = "null" ]; then
        id=$(jq -r '.release_id' "$OUTPUT_DIR/draft-assets.json" 2>/dev/null || true)
    fi
    if [ -z "$id" ] || [ "$id" = "null" ]; then
        id=$(git_find_release_id_by_tag "${DRAFT_RELEASE_TAG:-$RELEASE_TAG}" 2>/dev/null || true)
    fi
    if [ -z "$id" ] || [ "$id" = "null" ]; then
        id=$(git_find_draft_release_id_by_name "Release $RELEASE_TAG" 2>/dev/null || true)
    fi
    if [ -n "$id" ] && [ "$id" != "null" ]; then
        git_delete_release "$id"
        echo "Deleted release $id"
    else
        echo "No release id found to delete"
    fi
    if [ -n "${DRAFT_RELEASE_TAG:-}" ]; then
        git_delete_remote_tag "$DRAFT_RELEASE_TAG"
    fi
}

case "$ACTION" in
    draft) cmd_draft ;;
    tag) cmd_tag ;;
    publish) cmd_publish ;;
    candidate) cmd_candidate ;;
    promote) cmd_promote ;;
    rollback) cmd_rollback ;;
    delete) cmd_delete ;;
    delete-resources) cmd_delete_resources ;;
    *) echo "Unknown action: $ACTION"; exit 1 ;;
esac
