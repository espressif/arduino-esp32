#!/bin/bash
CHANGED_FILES=$1
echo "Pushing '$CHANGED_FILES' as $GITHUB_ACTOR"
git config --global github.user "$GITHUB_ACTOR"
git config --global user.name "$GITHUB_ACTOR"
git config --global user.email "$GITHUB_ACTOR@users.noreply.github.com"
for tool in $CHANGED_FILES; do
	git add tools/$tool.exe
done
git commit -m "Push binary to tools"
git push
