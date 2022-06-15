#/bin/bash
set -e

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

#git_remove_from_pages <file>
function git_remove_from_pages(){
    local path=$1
    local info=`curl -s -k -H "Authorization: token $GITHUB_TOKEN" -H "Accept: application/vnd.github.v3.object+json" -X GET "https://api.github.com/repos/$GITHUB_REPOSITORY/contents/$path?ref=gh-pages"`
    local type=`echo "$info" | jq -r '.type'`
    if [ ! $type == "file" ]; then
        if [ ! $type == "null" ]; then
            echo "Wrong type '$type'"
        else
            echo "File is not on Pages"
        fi
        return 0
    fi
    local sha=`echo "$info" | jq -r '.sha'`
    local message="Deleting "$(basename $path)
    local json="{\"branch\":\"gh-pages\",\"message\":\"$message\",\"sha\":\"$sha\"}"
    echo "$json" | curl -s -k -H "Authorization: token $GITHUB_TOKEN" -H "Accept: application/vnd.github.v3.raw+json" -X DELETE --data @- "https://api.github.com/repos/$GITHUB_REPOSITORY/contents/$path"
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

git_safe_upload_to_pages "index.md" "README.md"

# At some point github stopped providing a list of edited file
# but we also stopped havong documentation in md format,
# so we can skip this portion safely and update just the index

# EVENT_JSON=`cat $GITHUB_EVENT_PATH`

# echo "GITHUB_EVENT_PATH: $GITHUB_EVENT_PATH"
# echo "EVENT_JSON: $EVENT_JSON"

# pages_added=`echo "$EVENT_JSON" | jq -r '.commits[].added[]'`
# echo "added: $pages_added"
# pages_modified=`echo "$EVENT_JSON" | jq -r '.commits[].modified[]'`
# echo "modified: $pages_modified"
# pages_removed=`echo "$EVENT_JSON" | jq -r '.commits[].removed[]'`
# echo "removed: $pages_removed"

# for page in $pages_added; do
#     if [[ $page != "README.md" && $page != "docs/"* ]]; then
#         continue
#     fi
#     echo "Adding '$page' to pages ..."
#     if [[ $page == "README.md" ]]; then
#         git_safe_upload_to_pages "index.md" "README.md"
#     else
#         git_safe_upload_to_pages "$page" "$page"
#     fi
# done

# for page in $pages_modified; do
#     if [[ $page != "README.md" && $page != "docs/"* ]]; then
#         continue
#     fi
#     echo "Modifying '$page' ..."
#     if [[ $page == "README.md" ]]; then
#         git_safe_upload_to_pages "index.md" "README.md"
#     else
#         git_safe_upload_to_pages "$page" "$page"
#     fi
# done

# for page in $pages_removed; do
#     if [[ $page != "README.md" && $page != "docs/"* ]]; then
#         continue
#     fi
#     echo "Removing '$page' from pages ..."
#     if [[ $page == "README.md" ]]; then
#         git_remove_from_pages "README.md" > /dev/null
#     else
#         git_remove_from_pages "$page" > /dev/null
#     fi
# done

echo
echo "DONE!"
