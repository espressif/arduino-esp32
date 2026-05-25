#!/bin/bash
# Common functions for GitHub Release operations
# This library is sourced by release scripts
# Disable shellcheck warning about $? uses.
# shellcheck disable=SC2181

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
    local file="$1"
    local release_id="$2"
    local name
    name=$(basename "$file")
    # local mime=$(file -b --mime-type "$file")
    curl -X POST -sH "Authorization: token $GITHUB_TOKEN" -H "Content-Type: application/octet-stream" --data-binary @"$file" "https://uploads.github.com/repos/$GITHUB_REPOSITORY/releases/$release_id/assets?name=$name"
}

function git_safe_upload_asset {
    local file="$1"
    local release_id="$2"
    local name
    local size
    local upload_res

    name=$(basename "$file")
    size=$(get_file_size "$file")

    if ! upload_res=$(git_upload_asset "$file" "$release_id"); then
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

    info=$(curl -s -H "Authorization: token $GITHUB_TOKEN" -H "Accept: application/vnd.github.v3.object+json" -X GET "https://api.github.com/repos/$GITHUB_REPOSITORY/contents/$path?ref=gh-pages")
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

    echo "$data" | curl -s -H "Authorization: token $GITHUB_TOKEN" -H "Accept: application/vnd.github.v3.raw+json" -X PUT --data @- "https://api.github.com/repos/$GITHUB_REPOSITORY/contents/$path"
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
