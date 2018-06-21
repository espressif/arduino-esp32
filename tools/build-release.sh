#!/bin/bash

############################################################
# $1 - download link
# $2 - JSON output file 
function downloadAndMergePackageJSON()
{
	echo " --- Package JSON definition merge BEGIN ---"
	
	jsonLink=$1
	jsonOut=$2
	curlAuthToken=$3
	outDirectory=$4
	
	echo "Remote package JSON: $jsonLink (source)"
	echo "Current package JSON: $jsonOut (target)"
	
	old_json=$outDirectory/oldJson.json
	merged_json=$outDirectory/mergedJson.json
	
	#DEBUG
	#echo "     Local tmp for remote JSON: $old_json"
	#echo "     Merge output JSON: $merged_json"
	
	echo "Downloading JSON package definition: $jsonLink ..."

	# Authentication through HTTP headers might fail on redirection due to bug in cURL (https://curl.haxx.se/docs/adv_2018-b3bf.html - headers are resent to the target location including the original authentication)
	# Notes:
	#  - eg AmazonAWS fails with Bad Request due to having maximum 1 authentication mechanism per a request, might be general issue
	#  - it's a first-class credential leakage
	#  - the fix is available in cURL 7.58.0+, however, TravisCI is not yet updated (May 29, 2018) - see https://docs.travis-ci.com/user/build-environment-updates
	#  - TravisCI workaround: updating build environment through .travis.yml (ie install required version of cURL using apt-get, see https://docs.travis-ci.com/user/installing-dependencies/) 
	#  - previous point not used on purpose (build time increase, possible failure corrupts whole build, etc) but it's good to know there's a way out of hell
	#  - local workaround: authentication through 'access_token' as GET parameter works smoothly, however, HTTP headers are preferred
	
	#curl --verbose -sH "Authorization: token $curlAuthToken" -L -o "$old_json" "$jsonLink"
	curl -L -o "$old_json" "$jsonLink?access_token=$curlAuthToken"
	
	#curl -L -o "$old_json" "$jsonLink"
	
	echo "Merging $old_json into $jsonOut ..."
	
	echo
	set +e
	stdbuf -oL python package/merge_packages.py "$jsonOut" "$old_json" > "$merged_json"
	set -e	#supposed to be ON by default
	echo
	
	set -v
	if [ ! -s $merged_json ]; then
		rm -f "$merged_json"
		echo "Nothing to merge ($merged_json empty), $jsonOut unchanged"
	else
		rm -f "$jsonOut"
		mv "$merged_json" "$jsonOut"
		echo "Data successfully merged to $jsonOut"
	fi

	rm -f "$old_json"
	set +v
	echo " --- Package JSON definition merge END ---"
}
############################################################

#Cmdline options
# -a: GitHub API access token
# -d: output directory to store the (pre)release filedir set

set -e

echo
echo "==================================================================="
echo "RELEASE PACKAGE PUBLISHING ARRANGEMENTS (GitHub/Arduino compliance)"
echo "==================================================================="
echo

# cURL authentication token
while getopts ":a:,:d:" opt; do
  case $opt in
	a)
	  curlAuth=$OPTARG
	  echo "ACCESS TOKEN: $curlAuth" >&2
	  ;;
	d)
	  releaseDir=$OPTARG
	  echo "RELEASE OUTPUT DIRECTORY: $releaseDir" >&2
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

# where we at?
if [ -z "$TRAVIS_BUILD_DIR" ]; then
	echo "Non-TravisCI environment"
    cd "$( dirname ${BASH_SOURCE[0]} )"/..
	bTravisRun=0
else
	echo "TravisCI run"
	cd $TRAVIS_BUILD_DIR
	bTravisRun=1
fi

# no tag, no love
if [ -z "$TRAVIS_TAG" ] && [ $bTravisRun -eq 1 ]; then
	echo "Non-tagged builds not supported in Travis CI environment, exiting"
	exit 0
fi
	
currentDir=`pwd`
echo "Current working directory: $currentDir"

srcdir=$currentDir

if [ -z "$releaseDir" ]; then
	releaseDir=release
fi
echo "Release output directory: $releaseDir"


# get current branch name and commit hash
branch_name=""
verx=""
extent=""

if [ -z "$TRAVIS_TAG" ]; then	
	branch_name=`git rev-parse --abbrev-ref HEAD 2>/dev/null`
	ver=`sed -n -E 's/version=([0-9.]+)/\1/p' platform.txt`
	verx=`git rev-parse --short=8 HEAD 2>/dev/null`
else 
	ver=$TRAVIS_TAG
fi

# Package name (case-insensitive)
shopt -s nocasematch

if [ ! -z "$branch_name" ] && [ "$branch_name" != "master" ]; then
	extent="-$branch_name-$verx"
fi
	
package_name=esp32-$ver$extent

shopt -u nocasematch

echo "Version: $ver"
echo "Branch name: $branch_name"
echo "Git revision (8B): $verx"
echo "Extension: $extent"	
echo "Travis CI tag: $TRAVIS_TAG"
echo "Package name: $package_name"

# Set REMOTE_URL environment variable to the address where the package will be
# available for download. This gets written into package json file.

if [ -z "$REMOTE_URL" ]; then
    REMOTE_URL="http://localhost:8000"
    echo "REMOTE_URL not defined, using default"
fi

echo "Remote: $REMOTE_URL"

# Create directory for the package
outdir=$releaseDir/$package_name
echo "Temporary output directory: $outdir"

rm -rf $releaseDir
mkdir -p $outdir

# Copy package required stuff:

# <package root>
cp -f $srcdir/boards.txt $outdir/
cp -f $srcdir/platform.txt $outdir/
cp -f $srcdir/programmers.txt $outdir/

# <complete dirs> 
#	cores/
#	libraries/
#	variants/
cp -Rf $srcdir/cores $outdir/
cp -Rf $srcdir/libraries $outdir/
cp -Rf $srcdir/variants $outdir/

# <dir & files>
#	tools/partitions/
mkdir -p $outdir/tools/partitions
cp -f $srcdir/tools/partitions/boot_app0.bin $outdir/tools/partitions
cp -f $srcdir/tools/partitions/default.csv $outdir/tools/partitions
cp -f $srcdir/tools/partitions/minimal.csv $outdir/tools/partitions
cp -f $srcdir/tools/partitions/min_spiffs.csv $outdir/tools/partitions
cp -f $srcdir/tools/partitions/no_ota.csv $outdir/tools/partitions

#	tools/sdk/
cp -Rf $srcdir/tools/sdk $outdir/tools/

#	tools/
cp -f $srcdir/tools/espota.exe $outdir/tools/
cp -f $srcdir/tools/espota.py $outdir/tools/
cp -f $srcdir/tools/esptool.py $outdir/tools/
cp -f $srcdir/tools/gen_esp32part.py $outdir/tools/
cp -f $srcdir/tools/gen_esp32part.exe $outdir/tools/

find $outdir -name '*.DS_Store' -exec rm -f {} \;

# Do some replacements in platform.txt file, which are required because IDE
# handles tool paths differently when package is installed in hardware folder
cat $srcdir/platform.txt | \
sed 's/runtime.tools.xtensa-esp32-elf-gcc.path={runtime.platform.path}\/tools\/xtensa-esp32-elf//g' | \
sed 's/tools.esptool.path={runtime.platform.path}\/tools\/esptool/tools.esptool.path=\{runtime.tools.esptool.path\}/g' \
 > $outdir/platform.txt
 
# Put core version and short hash of git version into core_version.h
ver_define=`echo $plain_ver | tr "[:lower:].\055" "[:upper:]_"`
echo Ver define: $ver_define
echo \#define ARDUINO_ESP32_GIT_VER 0x`git rev-parse --short=8 HEAD 2>/dev/null` >$outdir/cores/esp32/core_version.h
echo \#define ARDUINO_ESP32_GIT_DESC `git describe --tags 2>/dev/null` >>$outdir/cores/esp32/core_version.h
echo \#define ARDUINO_ESP32_RELEASE_$ver_define >>$outdir/cores/esp32/core_version.h
echo \#define ARDUINO_ESP32_RELEASE \"$ver_define\" >>$outdir/cores/esp32/core_version.h
 
# Store submodules' current versions
git submodule status > $releaseDir/submodules.txt

# remove all .git* files
find $outdir -name '*.git*' -type f -delete

# Zip the package
package_name_zip=$package_name.zip

echo "----------------------------------------------------------"
echo "Making $package_name ZIP archive..."
echo

pushd $releaseDir >/dev/null

zip -qr $package_name_zip $package_name


echo "----------------------------------------------------------"
echo "Making $package_name JSON definition file(s)..."
echo

# Calculate SHA sum and size
sha=`shasum -a 256 $package_name_zip | cut -f 1 -d ' '`
size=`/bin/ls -l $package_name_zip | awk '{print $5}'`	
echo Size: $size
echo SHA-256: $sha

popd >/dev/null

PACKAGE_JSON_DEV="package_esp32_dev_index.json"
PACKAGE_JSON_REL="package_esp32_index.json"

# figure out the package type (release / pre-release)
shopt -s nocasematch
if [[ $TRAVIS_TAG == *-RC* ]]; then
	bIsPrerelease=1
	package_name_json=$PACKAGE_JSON_DEV
	echo "Package type: PRE-RELEASE, JSON def.file: $PACKAGE_JSON_DEV"
else
	bIsPrerelease=0
	package_name_json=$PACKAGE_JSON_REL
	echo "Package type: RELEASE, JSON def.files: $PACKAGE_JSON_REL, $PACKAGE_JSON_DEV"
fi
shopt -u nocasematch

# Generate JSON package definition
echo
echo "----------------------------------------------------------"
echo "Preparing current package definition ($package_name_json)..."

# JSON contents
jq_arg=".packages[0].platforms[0].version = \"$ver\" | \
    .packages[0].platforms[0].url = \"$REMOTE_URL/$package_name_zip\" |\
    .packages[0].platforms[0].archiveFileName = \"$package_name_zip\""

jq_arg="$jq_arg |\
	.packages[0].platforms[0].size = \"$size\" |\
	.packages[0].platforms[0].checksum = \"SHA-256:$sha\""


# Cleanup temporary work dir
rm -rf $outdir


# Get previous release name
echo
echo "----------------------------------------------------------"
echo "Getting previous releases versions..."

releasesJson=$releaseDir/releases.json

curl -sH "Authorization: token $curlAuth" https://api.github.com/repos/$TRAVIS_REPO_SLUG/releases > $releasesJson

# Previous final release (prerelase == false)
prev_release=$(jq -r '. | map(select(.draft == false and .prerelease == false)) | sort_by(.created_at | - fromdateiso8601) | .[0].tag_name' ${releasesJson})
# Previous release (possibly a pre-release)
prev_any_release=$(jq -r '. | map(select(.draft == false)) | sort_by(.created_at | - fromdateiso8601)  | .[0].tag_name' ${releasesJson})
# Previous pre-release
prev_pre_release=$(jq -r '. | map(select(.draft == false and .prerelease == true)) | sort_by(.created_at | - fromdateiso8601)  | .[0].tag_name' ${releasesJson})

rm -f "$releasesJson"

echo "Previous release: $prev_release"
echo "Previous (pre-?)release: $prev_any_release"
echo "Previous pre-release: $prev_pre_release"

# always get DEV version of JSON (included in both RC/REL)
echo
echo "----------------------------------------------------------"
echo "Generating $PACKAGE_JSON_DEV..."
echo

cat $srcdir/package/package_esp32_index.template.json | jq "$jq_arg" > $releaseDir/$PACKAGE_JSON_DEV
if [ ! -z "$prev_any_release" ] && [ "$prev_any_release" != "null" ]; then
	downloadAndMergePackageJSON "https://github.com/$TRAVIS_REPO_SLUG/releases/download/${prev_any_release}/${PACKAGE_JSON_DEV}" "$releaseDir/${PACKAGE_JSON_DEV}" "${curlAuth}" "$releaseDir"
	
	# Release notes: GIT log comments (prev_any_release, current_release>
	git log --oneline $prev_any_release.. > $releaseDir/commits.txt
fi	

# for RELEASE run update REL JSON as well
if [ $bIsPrerelease -eq 0 ]; then

	echo
	echo "----------------------------------------------------------"
	echo "Generating $PACKAGE_JSON_REL..."
	echo

	cat $srcdir/package/package_esp32_index.template.json | jq "$jq_arg" > $releaseDir/$PACKAGE_JSON_REL
	if [ ! -z "$prev_release" ] && [ "$prev_release" != "null" ]; then
		downloadAndMergePackageJSON "https://github.com/$TRAVIS_REPO_SLUG/releases/download/${prev_release}/${PACKAGE_JSON_REL}" "$releaseDir/${PACKAGE_JSON_REL}" "${curlAuth}" "$releaseDir"
		
		# Release notes: GIT log comments (prev_release, current_release>
		git log --oneline $prev_release.. > $releaseDir/commits.txt
	fi
fi

echo
echo "=============================================================="
echo "Package '$package_name' ready for publishing, script finished."
echo "=============================================================="
echo
