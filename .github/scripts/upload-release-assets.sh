#!/bin/bash
# Upload package JSONs and push version commit
# Disable shellcheck warning about $? uses.
# shellcheck disable=SC2181

set -e

SCRIPTS_DIR="./.github/scripts"

# Source common GitHub release functions
source "$SCRIPTS_DIR/lib-github-release.sh"

OUTPUT_DIR="$GITHUB_WORKSPACE/build"
PACKAGE_JSON_DEV="package_esp32_dev_index.json"
PACKAGE_JSON_REL="package_esp32_index.json"
PACKAGE_JSON_DEV_CN="package_esp32_dev_index_cn.json"
PACKAGE_JSON_REL_CN="package_esp32_index_cn.json"

echo "Uploading package JSONs ..."

echo "Uploading $PACKAGE_JSON_DEV ..."
echo "Download URL: $(git_safe_upload_asset "$OUTPUT_DIR/$PACKAGE_JSON_DEV" "$RELEASE_ID")"
echo "Pages URL: $(git_safe_upload_to_pages "$PACKAGE_JSON_DEV" "$OUTPUT_DIR/$PACKAGE_JSON_DEV")"
echo "Download CN URL: $(git_safe_upload_asset "$OUTPUT_DIR/$PACKAGE_JSON_DEV_CN" "$RELEASE_ID")"
echo "Pages CN URL: $(git_safe_upload_to_pages "$PACKAGE_JSON_DEV_CN" "$OUTPUT_DIR/$PACKAGE_JSON_DEV_CN")"
echo

if [ "$RELEASE_PRE" == "false" ]; then
    echo "Uploading $PACKAGE_JSON_REL ..."
    echo "Download URL: $(git_safe_upload_asset "$OUTPUT_DIR/$PACKAGE_JSON_REL" "$RELEASE_ID")"
    echo "Pages URL: $(git_safe_upload_to_pages "$PACKAGE_JSON_REL" "$OUTPUT_DIR/$PACKAGE_JSON_REL")"
    echo "Download CN URL: $(git_safe_upload_asset "$OUTPUT_DIR/$PACKAGE_JSON_REL_CN" "$RELEASE_ID")"
    echo "Pages CN URL: $(git_safe_upload_to_pages "$PACKAGE_JSON_REL_CN" "$OUTPUT_DIR/$PACKAGE_JSON_REL_CN")"
    echo
fi

# Update version and push if needed
echo "Updating version..."
if ! "${SCRIPTS_DIR}/update-version.sh" "$RELEASE_TAG"; then
    echo "ERROR: update_version failed!"
    exit 1
fi

git config user.name "github-actions[bot]"
git config user.email "41898282+github-actions[bot]@users.noreply.github.com"
git add .

# Check if there are changes to commit
if git diff --cached --quiet; then
    echo "Version already updated, no commit needed."
else
    echo "Creating version update commit..."
    git commit -m "change(version): Update core version to $RELEASE_TAG"

    echo "Pushing version update commit..."
    git push
    new_tag_commit=$(git rev-parse HEAD)
    echo "New commit: $new_tag_commit"

    echo "Moving tag $RELEASE_TAG to $new_tag_commit..."
    git tag -f "$RELEASE_TAG" "$new_tag_commit"
    git push --force origin "$RELEASE_TAG"

    echo "Version commit pushed successfully!"
fi

echo "Upload complete!"
