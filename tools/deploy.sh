#!/bin/bash

set -e

#Cmdline options
# -t: tag (*_RC* determines prerelease version, can be overriden be -p)
# -a: GitHub API access token
# -s: GitHub repository slug (user/repo)
# -p: prerelease true/false
# -f: files to upload (ie assets. delim = ';', must come quoted)
# -d: directory to upload (by adding dir contents to assets)
while getopts ":t:,:a:,:s:,:p:,:f:,:d:" opt; do
  case $opt in
	t)
	  varTagName=$OPTARG
	  echo "TAG: $varTagName" >&2
	  ;;
	a)
	  varAccessToken=$OPTARG
	  echo "ACCESS TOKEN: $varAccessToken" >&2
	  ;;
	s)
	  varRepoSlug=$OPTARG
	  echo "REPO SLUG: $varRepoSlug" >&2
	  ;;
	p)
	  varPrerelease=$OPTARG
	  echo "PRERELEASE: $varPrerelease" >&2
	  ;;
	f)
	  varAssets=$OPTARG
	  echo "ASSETS: $varAssets" >&2
	  ;;	  
	d)
	  varAssetsDir=$OPTARG
	  echo "ASSETS DIR: $varAssetsDir" >&2
	  ;;	  
	\?)
	  echo "Invalid option: -$OPTARG" >&2
	  exit 1
	  ;;
	:)
	  echo "Option -$OPTARG requires an argument." >&2
	  exit 1
	  ;;
  esac
done

#Check tag name for release/prerelease (prerelease tag contains '_RC' as for release-candidate. case-insensitive)
shopt -s nocasematch
if [ -z $varPrerelease ]; then
	if [[ $varTagName == *-RC* ]]; then
		varPrerelease=true
	else
		varPrerelease=false
	fi
fi
shopt -u nocasematch

#
# Prepare Markdown release notes:
#################################
#
# - tag's description:
# 	ignore first 3 lines - commiter, tagname, blank
#	first line of message: heading
#	other lines: converted to bullets
#	empty lines ignored
#	if '* ' found as a first char pair, it's converted to '- ' to keep bulleting unified
relNotesRaw=`git show -s --format=%b $varTagName`
readarray -t msgArray <<<"$relNotesRaw"

arrLen=${#msgArray[@]}
if [ $arrLen > 3 ]; then 
	ind=3
	while [ $ind -lt $arrLen ]; do
		if [ $ind -eq 3 ]; then
			releaseNotes="#### ${msgArray[ind]}\\n"
		else
			oneLine="$(echo -e "${msgArray[ind]}" | sed -e 's/^[[:space:]]*//')"
			
			if [ ${#oneLine} -gt 0 ]; then
				if [ "${oneLine:0:2}" == "* " ]; then oneLine=$(echo ${oneLine/\*/-}); fi
				if [ "${oneLine:0:2}" != "- " ]; then releaseNotes+="- "; fi		
				releaseNotes+="$oneLine\\n"
			fi
		fi
		let ind=$ind+1
	done
else
	releaseNotes="#### Release of $varTagName\\n"
fi

# - list of commits (commits.txt must exit in the output dir)
commitFile=$varAssetsDir/commits.txt
if [ -e "$commitFile" ]; then
	
	releaseNotes+="\\n##### Commits\\n"
	
	IFS=$'\n'
	for next in `cat $commitFile`
	do
		IFS=' ' read -r commitId commitMsg <<< "$next"
		releaseNotes+="- [$commitId](https://github.com/$varRepoSlug/commit/$commitId) $commitMsg\\n"
	done
	rm -f $commitFile
fi

releaseNotes=$(perl -pe 's/\r?\n/\\n/' <<< ${releaseNotes})

#JSON parameters to create a new release
curlData="{\"tag_name\": \"$varTagName\",\"target_commitish\": \"master\",\"name\": \"v$varTagName\",\"body\": \"$releaseNotes\",\"draft\": false,\"prerelease\": $varPrerelease}"

#Create the release (initial source file assets created by GitHub)
releaseId=$(curl --data "$curlData" https://api.github.com/repos/$varRepoSlug/releases?access_token=$varAccessToken | jq -r '.id')
echo Release ID: $releaseId

# Assets defined by dir contents
if [ ! -z $varAssetsDir ]; then
	varAssetsTemp=$(ls -p $varAssetsDir | grep -v / | tr '\n' ';')
	for item in $(echo $varAssetsTemp | tr ";" "\n")
	do	  
	  varAssets+=$varAssetsDir/$item;
	  varAssets+=';'
	done	
fi

echo
echo varAssets: $varAssets

#Upload additional assets
if [ ! -z $varAssets ]; then
	curlAuth="Authorization: token $varAccessToken"
	for filename in $(echo $varAssets | tr ";" "\n")
	do
	  echo
	  echo
	  echo Uploading $filename...
	  
	  curl -X POST -sH "$curlAuth" -H "Content-Type: application/octet-stream" --data-binary @"$filename" https://uploads.github.com/repos/$varRepoSlug/releases/$releaseId/assets?name=$(basename $filename)
	done
fi

echo
echo
	

