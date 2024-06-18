#!/bin/bash
CHANGED_FILES=$1
echo "Pushing '$CHANGED_FILES' as github-actions[bot]"
git config --global github.user "github-actions[bot]"
git config --global user.name "github-actions[bot]"
git config --global user.email "41898282+github-actions[bot]@users.noreply.github.com"
for tool in $CHANGED_FILES; do
	git add tools/$tool.exe
done
git commit -m "change(tools): Push generated binaries to PR"
git push
