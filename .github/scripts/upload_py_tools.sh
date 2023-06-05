#!/bin/bash
echo "Pushing as $GITHUB_ACTOR"
git config --global github.user "$GITHUB_ACTOR"
git config --global user.name "$GITHUB_ACTOR"
git config --global user.email "$GITHUB_ACTOR@users.noreply.github.com"
git add tools/get.exe
git commit -m "Push binary to tools"
git push

