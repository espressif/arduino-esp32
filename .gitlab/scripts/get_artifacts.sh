#!/bin/bash
# Disable shellcheck warning about $? uses.
# shellcheck disable=SC2181

set -e
set -o pipefail

echo "Downloading test binaries for $TEST_CHIP from GitHub repository $GITHUB_REPOSITORY"
echo "Binaries run ID: $BINARIES_RUN_ID"
echo "Looking for artifact: test-bin-$TEST_CHIP-$TEST_TYPE"

# Check if GitHub token is available
if [ -z "$GITHUB_DOWNLOAD_PAT" ]; then
    echo "ERROR: GITHUB_DOWNLOAD_PAT not available in GitLab environment"
    echo "Please set up GITHUB_DOWNLOAD_PAT in GitLab CI/CD variables"
    exit 1
fi

# First, get the artifacts list and save it for debugging
echo "Fetching artifacts list from GitHub API..."
artifacts_response=$(curl -s -H "Authorization: token $GITHUB_DOWNLOAD_PAT" \
    -H "Accept: application/vnd.github.v3+json" \
    "https://api.github.com/repos/$GITHUB_REPOSITORY/actions/runs/$BINARIES_RUN_ID/artifacts")

# Check if we got a valid response
if [ -z "$artifacts_response" ]; then
    echo "ERROR: Empty response from GitHub API"
    exit 1
fi

# Check for API errors
error_message=$(echo "$artifacts_response" | jq -r '.message // empty' 2>/dev/null)
if [ -n "$error_message" ]; then
    echo "ERROR: GitHub API returned error: $error_message"
    exit 1
fi

# Find the download URL for our specific artifact
download_url=$(echo "$artifacts_response" | jq -r ".artifacts[] | select(.name==\"test-bin-$TEST_CHIP-$TEST_TYPE\") | .archive_download_url" 2>/dev/null)

if [ "$download_url" = "null" ] || [ -z "$download_url" ]; then
    echo "ERROR: Could not find artifact 'test-bin-$TEST_CHIP-$TEST_TYPE'"
    echo "This could mean:"
    echo "1. The artifact name doesn't match exactly"
    echo "2. The artifacts haven't been uploaded yet"
    echo "3. The GitHub run ID is incorrect"
    exit 1
fi

echo "Found download URL: $download_url"

# Download the artifact
echo "Downloading artifact..."
curl -H "Authorization: token $GITHUB_DOWNLOAD_PAT" -L "$download_url" -o test-binaries.zip

if [ $? -ne 0 ] || [ ! -f test-binaries.zip ]; then
    echo "ERROR: Failed to download artifact"
    exit 1
fi

echo "Extracting binaries..."
unzip -q -o test-binaries.zip -d ~/.arduino/tests/"$TEST_CHIP"/

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to extract binaries"
    exit 1
fi

rm -f test-binaries.zip
echo "Successfully downloaded and extracted test binaries"
