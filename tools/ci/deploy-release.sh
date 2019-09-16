#!/bin/bash


json_escape () {
    printf '%s' "$1" | python -c 'import json,sys; print(json.dumps(sys.stdin.read()))'
	#printf '%s' "$1" | php -r 'echo json_encode(file_get_contents("php://stdin"));'
}

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

# use TravisCI env as default, if available
if [ -z $varTagName ] && [ ! -z $TRAVIS_TAG ]; then
	varTagName=$TRAVIS_TAG
fi

if [ -z $varTagName ]; then
	echo "No tag name available => aborting"
	exit 1
fi

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
# - annotated tags only, lightweight tags just display message of referred commit
# - tag's description conversion to relnotes:
# 	first 3 lines (tagname, commiter, blank): ignored
#	4th line: relnotes heading
#	remaining lines: each converted to bullet-list item
#	empty lines ignored
#	if '* ' found as a first char pair, it's converted to '- ' to keep bulleting unified
echo
echo Preparing release notes
echo -----------------------
echo "Tag's message:"

relNotesRaw=`git show -s --format=%b $varTagName`
readarray -t msgArray <<<"$relNotesRaw"
arrLen=${#msgArray[@]}

#process annotated tags only
if [ $arrLen > 3 ] && [ "${msgArray[0]:0:3}" == "tag" ]; then 
	ind=3
	while [ $ind -lt $arrLen ]; do
		if [ $ind -eq 3 ]; then
			releaseNotes="#### ${msgArray[ind]}"
			releaseNotes+=$'\r\n'
		else
			oneLine="$(echo -e "${msgArray[ind]}" | sed -e 's/^[[:space:]]*//')"
			
			if [ ${#oneLine} -gt 0 ]; then
				if [ "${oneLine:0:2}" == "* " ]; then oneLine=$(echo ${oneLine/\*/-}); fi
				if [ "${oneLine:0:2}" != "- " ]; then releaseNotes+="- "; fi		
				releaseNotes+="$oneLine"
				releaseNotes+=$'\r\n'
				
				#debug output
				echo "   ${oneLine}"
			fi
		fi
		let ind=$ind+1
	done
fi

echo "$releaseNotes"

# - list of commits (commits.txt must exit in the output dir)
commitFile=$varAssetsDir/commits.txt
if [ -s "$commitFile" ]; then
	
	releaseNotes+=$'\r\n##### Commits\r\n'
	
	echo
	echo "Commits:"
		
	IFS=$'\n'
	for next in `cat $commitFile`
	do
		IFS=' ' read -r commitId commitMsg <<< "$next"
		commitLine="- [$commitId](https://github.com/$varRepoSlug/commit/$commitId) $commitMsg"
		echo "   $commitLine"
		
		releaseNotes+="$commitLine"
		releaseNotes+=$'\r\n'
	done
	rm -f $commitFile
fi

# Check possibly existing release for current tag 
echo
echo "Processing GitHub release record for $varTagName:"
echo "-------------------------------------------------"

echo " - check $varTagName possible existence..."

# (eg build invoked by Create New Release GHUI button -> GH default release pack created immediately including default assests)
HTTP_RESPONSE=$(curl -L --silent --write-out "HTTPSTATUS:%{http_code}" https://api.github.com/repos/$varRepoSlug/releases/tags/$varTagName?access_token=$varAccessToken)
if [ $? -ne 0 ]; then echo "FAILED: $? => aborting"; exit 1; fi

HTTP_BODY=$(echo $HTTP_RESPONSE | sed -e 's/HTTPSTATUS\:.*//g')
HTTP_STATUS=$(echo $HTTP_RESPONSE | tr -d '\n' | sed -e 's/.*HTTPSTATUS://')
echo " ---> GitHub server HTTP response: $HTTP_STATUS"

# if the release exists, append/update recent files to its assets vector
if [ $HTTP_STATUS -eq 200 ]; then
	releaseId=$(echo $HTTP_BODY | jq -r '.id')
	echo " - $varTagName release found (id $releaseId)"
	
	#Merge release notes and overwrite pre-release flag. all other attributes remain unchanged:
	
	# 1. take existing notes from server (added by release creator)
	releaseNotesGH=$(echo $HTTP_BODY | jq -r '.body')
	
	# - strip possibly trailing CR
	if [ "${releaseNotesGH: -1}" == $'\r' ]; then 		
		releaseNotesTemp="${releaseNotesGH:0:-1}"
	else 
		releaseNotesTemp="$releaseNotesGH"
	fi
	# - add CRLF to make relnotes consistent for JSON encoding
	releaseNotesTemp+=$'\r\n'
	
	# 2. #append generated relnotes (usually commit oneliners)
	releaseNotes="$releaseNotesTemp$releaseNotes"
	
	# 3. JSON-encode whole string for GH API transfer
	releaseNotes=$(json_escape "$releaseNotes")
	
	# 4. remove extra quotes returned by python (dummy but whatever)
	releaseNotes=${releaseNotes:1:-1}
	
	#Update current GH release record
	echo " - updating release notes and pre-release flag:"
	
	curlData="{\"body\": \"$releaseNotes\",\"prerelease\": $varPrerelease}"
	echo "   <data.begin>$curlData<data.end>"
	echo
	#echo "DEBUG: curl --data \"$curlData\" https://api.github.com/repos/$varRepoSlug/releases/$releaseId?access_token=$varAccessToken"
	
	curl --data "$curlData" https://api.github.com/repos/$varRepoSlug/releases/$releaseId?access_token=$varAccessToken
	if [ $? -ne 0 ]; then echo "FAILED: $? => aborting"; exit 1; fi
	
	echo " - $varTagName release record successfully updated"
	
#... or create a new release record
else 
	releaseNotes=$(json_escape "$releaseNotes")
	releaseNotes=${releaseNotes:1:-1}

	echo " - release $varTagName not found, creating a new record:"
	
	curlData="{\"tag_name\": \"$varTagName\",\"target_commitish\": \"master\",\"name\": \"v$varTagName\",\"body\": \"$releaseNotes\",\"draft\": false,\"prerelease\": $varPrerelease}"
	echo "   <data.begin>$curlData<data.end>"
	
	#echo "DEBUG: curl --data \"${curlData}\" https://api.github.com/repos/${varRepoSlug}/releases?access_token=$varAccessToken | jq -r '.id'"
	releaseId=$(curl --data "$curlData" https://api.github.com/repos/$varRepoSlug/releases?access_token=$varAccessToken | jq -r '.id')
	if [ $? -ne 0 ]; then echo "FAILED: $? => aborting"; exit 1; fi	
	
	echo " - $varTagName release record successfully created (id $releaseId)"
fi

# Assets defined by dir contents
if [ ! -z $varAssetsDir ]; then
	varAssetsTemp=$(ls -p $varAssetsDir | grep -v / | tr '\n' ';')
	for item in $(echo $varAssetsTemp | tr ";" "\n")
	do	  
	  varAssets+=$varAssetsDir/$item;
	  varAssets+=';'
	done	
fi

#Upload additional assets
if [ ! -z $varAssets ]; then
	echo
	echo "Uploading assets:"
	echo "-----------------"
	echo " Files to upload:"
	echo "   $varAssets"
	echo
	
	curlAuth="Authorization: token $varAccessToken"
	for filename in $(echo $varAssets | tr ";" "\n")
	do
	  echo " - ${filename}:" 
	  curl -X POST -sH "$curlAuth" -H "Content-Type: application/octet-stream" --data-binary @"$filename" https://uploads.github.com/repos/$varRepoSlug/releases/$releaseId/assets?name=$(basename $filename)
	  
	  if [ $? -ne 0 ]; then echo "FAILED: $? => aborting"; exit 1; fi
	  
	  echo
	  echo "OK"
	  echo
	  
	done
fi
