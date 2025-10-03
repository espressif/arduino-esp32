#!/bin/bash

set -euo pipefail

echo "Collecting artifacts from child pipeline(s)"

api="$CI_API_V4_URL"
proj="$CI_PROJECT_ID"
parent="$CI_PIPELINE_ID"

# Choose auth header (prefer PRIVATE-TOKEN if provided)
AUTH_HEADER="JOB-TOKEN: $CI_JOB_TOKEN"
if [ -n "${GITLAB_API_TOKEN:-}" ]; then
    AUTH_HEADER="PRIVATE-TOKEN: $GITLAB_API_TOKEN"
fi

# Verify project is reachable
if ! curl -sf --header "$AUTH_HEADER" "$api/projects/$proj" >/dev/null; then
    echo "WARNING: Unable to access project $proj via API (token scope?)"
    exit 1
fi

bridges=$(curl -s --header "$AUTH_HEADER" "$api/projects/$proj/pipelines/$parent/bridges")
# Ensure we got a JSON array
if ! echo "$bridges" | jq -e 'type=="array"' >/dev/null 2>&1; then
    echo "WARNING: Unexpected bridges response:"; echo "$bridges"
    exit 1
fi

child_ids=$(echo "$bridges" | jq -r '.[] | select(.name=="trigger-hw-tests") | .downstream_pipeline.id')
mkdir -p aggregated

for cid in $child_ids; do
    echo "Child pipeline: $cid"

    jobs=$(curl -s --header "$AUTH_HEADER" "$api/projects/$proj/pipelines/$cid/jobs?per_page=100")
    if ! echo "$jobs" | jq -e 'type=="array"' >/dev/null 2>&1; then
        echo "WARNING: Unable to list jobs for child $cid"; echo "$jobs"
        exit 1
    fi

    ids=$(echo "$jobs" | jq -r '.[] | select(.artifacts_file!=null) | .id')
    failed=false
    for jid in $ids; do
        echo "Downloading artifacts from job $jid"
        curl --header "$AUTH_HEADER" -L -s "$api/projects/$proj/jobs/$jid/artifacts" -o artifact.zip || true
        if [ -f artifact.zip ]; then
            unzip -q -o artifact.zip -d . >/dev/null 2>&1 || true
        else
            echo "Job $jid has no artifacts"
            failed=true
        fi
        rm -f artifact.zip
    done
done

if $failed; then
    echo "Some jobs failed to download artifacts"
    exit 1
fi
