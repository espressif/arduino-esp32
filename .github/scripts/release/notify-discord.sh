#!/bin/bash
# Post a GitHub-style release event to Discord's /github webhook endpoint.
# Renders the same card as a native GitHub repo webhook (avatar + release link).
# shellcheck disable=SC2181

set -e

RELEASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$RELEASE_DIR/common.sh"

RELEASE_TAG="${RELEASE_TAG:?RELEASE_TAG required}"
RELEASE_TAG=$(normalize_release_tag "$RELEASE_TAG")

[ -n "${DISCORD_WEBHOOK_URL:-}" ] || {
    echo "ERROR: DISCORD_WEBHOOK_URL required" >&2
    exit 1
}
[ -n "${GITHUB_TOKEN:-}" ] && [ -n "${GITHUB_REPOSITORY:-}" ] || {
    echo "ERROR: GITHUB_TOKEN and GITHUB_REPOSITORY required" >&2
    exit 1
}

case "$DISCORD_WEBHOOK_URL" in
    */github) ;;
    *)
        echo "ERROR: DISCORD_WEBHOOK_URL must end with /github" >&2
        exit 1
        ;;
esac

RELEASE_API=$(curl -fsSL -H "Authorization: token $GITHUB_TOKEN" \
    "https://api.github.com/repos/$GITHUB_REPOSITORY/releases/tags/$RELEASE_TAG")
REPO_API=$(curl -fsSL -H "Authorization: token $GITHUB_TOKEN" \
    "https://api.github.com/repos/$GITHUB_REPOSITORY")

# Shape API objects like a GitHub release webhook payload (not the full REST response).
RELEASE_JSON=$(jq '{
    id, tag_name, target_commitish, name, body, draft, prerelease,
    created_at, published_at, html_url, tarball_url, zipball_url,
    author, assets
}' <<<"$RELEASE_API")
REPO_JSON=$(jq '{
    id, node_id, name, full_name, private, html_url, description, fork, url,
    owner: (.owner | {login, id, avatar_url, html_url, type})
}' <<<"$REPO_API")
SENDER_JSON=$(jq '{
    login, id, avatar_url, html_url, type
}' <<<"$(jq '.author' <<<"$RELEASE_API")")

PAYLOAD=$(jq -n \
    --argjson release "$RELEASE_JSON" \
    --argjson repository "$REPO_JSON" \
    --argjson sender "$SENDER_JSON" \
    '{action:"published", release:$release, repository:$repository, sender:$sender}')

RESPONSE_FILE="$(mktemp)"
HTTP_CODE=$(curl -sS -o "$RESPONSE_FILE" -w "%{http_code}" \
    -X POST "$DISCORD_WEBHOOK_URL" \
    -H "Content-Type: application/json" \
    -H "X-GitHub-Event: release" \
    -H "User-Agent: GitHub-Hookshot/release-notify" \
    -d "$PAYLOAD")

if [ "$HTTP_CODE" -lt 200 ] || [ "$HTTP_CODE" -ge 300 ]; then
    echo "ERROR: Discord returned HTTP $HTTP_CODE" >&2
    cat "$RESPONSE_FILE" >&2
    rm -f "$RESPONSE_FILE"
    exit 1
fi

rm -f "$RESPONSE_FILE"
echo "Discord notified for release $RELEASE_TAG (HTTP $HTTP_CODE)"
