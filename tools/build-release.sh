#!/bin/bash

############################################################
# $1 - download link
# $2 - JSON output file 
function downloadAndMergePackageJSON()
{
	echo
	echo " ---Package JSON definition merge BEGIN--->"
	
	jsonLink=$1
	jsonOut=$2
	curlAuthToken=$3
	outDirectory=$4
	
	echo " - remote package JSON: $jsonLink (source)"
	echo " - current package JSON: $jsonOut (target)"
	
	old_json=$outDirectory/oldJson.json
	merged_json=$outDirectory/mergedJson.json
	
	#DEBUG
	#echo "     Local tmp for remote JSON: $old_json"
	#echo "     Merge output JSON: $merged_json"
	
	echo " - downloading JSON package definition: $jsonLink ..."

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
	if [ $? -ne 0 ]; then echo "FAILED: $? => aborting"; exit 1; fi

	
	#curl -L -o "$old_json" "$jsonLink"
	
	echo " - merging $old_json into $jsonOut ..."
	
	echo
	set +e
	stdbuf -oL python package/merge_packages.py "$jsonOut" "$old_json" > "$merged_json"
	set -e	#supposed to be ON by default
	echo
	
	set -v
	if [ ! -s $merged_json ]; then
		rm -f "$merged_json"
		echo " Done: nothing to merge ($merged_json empty) => $jsonOut remains unchanged"
	else
		rm -f "$jsonOut"
		mv "$merged_json" "$jsonOut"
		echo " Done: JSON data successfully merged to $jsonOut"
	fi

	rm -f "$old_json"
	set +v
	echo " <---Package JSON definition merge END---"
	echo
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
cmdLine=`basename $0 $@`
echo "Cmdline: ${cmdLine}"

# cURL authentication token
while getopts ":a:,:d:" opt; do
  case $opt in
	a)
	  curlAuth=$OPTARG
	  #echo " ACCESS TOKEN: $curlAuth" >&2
	  ;;
	d)
	  releaseDir=$OPTARG
	  #echo " RELEASE OUTPUT DIRECTORY: $releaseDir" >&2
	  ;;
	\?)
	  echo "Error: invalid option -$OPTARG => aborting" >&2
	  exit 1
	  ;;
	:)
	  echo "Error: option -$OPTARG requires an argument => aborting" >&2
	  exit 1
	  ;;
  esac
done

# where we at?
echo
echo "Prequisite check:"
if [ -z "$TRAVIS_BUILD_DIR" ]; then
	echo " - non-TravisCI environment"
    cd "$( dirname ${BASH_SOURCE[0]} )"/..
	bTravisRun=0
else
	echo " - TravisCI run"
	cd $TRAVIS_BUILD_DIR
	bTravisRun=1
fi

# no tag, no love
if [ -z "$TRAVIS_TAG" ] && [ $bTravisRun -eq 1 ]; then
	echo "Warning: non-tagged builds not supported in Travis CI environment => exiting"
	exit 0
fi
	
echo
echo "Package build settings:"
echo "======================="

# source directory
srcdir=`pwd`
echo "Current working directory: ${srcdir}"

# target directory for actual release fileset
if [ -z "$releaseDir" ]; then
	releaseDir=release
fi
echo "Release output directory: $releaseDir"

# Git versions, branch names, tags
branch_name=""
verx=""
extent=""

if [ -z "$TRAVIS_TAG" ]; then	
	branch_name=`git rev-parse --abbrev-ref HEAD 2>/dev/null`
	ver=`sed -n -E 's/version=([0-9.]+)/\1/p' platform.txt`
else 
	ver=$TRAVIS_TAG
fi
verx=`git rev-parse --short=8 HEAD 2>/dev/null`

# Package name resolving (case-insensitive):
# - unknown branch, master branch or branch in detached state (HEAD revision) use only the tag's name as version string (esp32-$TAG_NAME, eg 'esp32-1.0.0-RC1')
# - all other branches use long-version string (esp32-$BRANCH_NAME-$GITREV_NUMBER_SHORT, eg 'esp32-idf_update-cde668da')

shopt -s nocasematch

if [ ! -z "$branch_name" ] && [ "$branch_name" != "master" ] && [ "$branch_name" != "head" ]; then
	extent="-$branch_name-$verx"
fi
	
package_name=esp32-$ver$extent

shopt -u nocasematch

echo "Package version: $ver"
echo "Git branch name: $branch_name"
echo "Git revision number: $verx"
echo "Package name extension: $extent"	
echo "Travis CI tag: $TRAVIS_TAG"
echo "Release package name: $package_name"

# Set REMOTE_URL environment variable to the address where the package will be
# available for download. This gets written into package json file.

if [ -z "$REMOTE_URL" ]; then
    REMOTE_URL="http://localhost:8000"
	remoteEchoOut="${REMOTE_URL} (REMOTE_URL variable not defined, using default)"
else 
	remoteEchoOut="${REMOTE_URL}"
fi
echo "Target URL for download (JSON incl): ${remoteEchoOut}"

# Create directory for the package
outdir=$releaseDir/$package_name
echo "Local temp directory: $outdir"

rm -rf $releaseDir
mkdir -p $outdir

# Copy files required for the package release:
echo
echo "Package build processing:"
echo "========================="
echo
echo "Prepare files for the package main archive:"
echo " - copying neccessary files from current Git repository..."

# <PACKAGE ROOT>
cp -f $srcdir/boards.txt $outdir/
cp -f $srcdir/platform.txt $outdir/
cp -f $srcdir/programmers.txt $outdir/

# <COMPLETE DIRS> 
#	cores/
#	libraries/
#	variants/
#	tools/partitions/
cp -Rf $srcdir/cores $outdir/
cp -Rf $srcdir/libraries $outdir/
cp -Rf $srcdir/variants $outdir/
mkdir -p $outdir/tools
cp -Rf $srcdir/tools/partitions $outdir/tools/

# <DIR & FILES>
#	tools/sdk/
cp -Rf $srcdir/tools/sdk $outdir/tools/

#	tools/
cp -f $srcdir/tools/espota.exe $outdir/tools/
cp -f $srcdir/tools/espota.py $outdir/tools/
cp -f $srcdir/tools/esptool.py $outdir/tools/
cp -f $srcdir/tools/gen_esp32part.py $outdir/tools/
cp -f $srcdir/tools/gen_esp32part.exe $outdir/tools/

echo " - cleaning *.DS_Store files..."
find $outdir -name '*.DS_Store' -exec rm -f {} \;

# Do some replacements in platform.txt file, which are required because IDE
# handles tool paths differently when package is installed in hardware folder
echo " - updating platform.txt..."
cat $srcdir/platform.txt | \
sed "s/version=.*/version=$ver$extent/g" | \
sed 's/runtime.tools.xtensa-esp32-elf-gcc.path={runtime.platform.path}\/tools\/xtensa-esp32-elf//g' | \
sed 's/tools.esptool.path={runtime.platform.path}\/tools\/esptool/tools.esptool.path=\{runtime.tools.esptool.path\}/g' \
 > $outdir/platform.txt

# Put core version and short hash of git version into core_version.h
ver_define=`echo $ver | tr "[:lower:].\055" "[:upper:]_"`
echo " - generating C/C++ header defines ($ver_define -> /cores/esp32/core_version.h)..."

echo \#define ARDUINO_ESP32_GIT_VER 0x$verx >$outdir/cores/esp32/core_version.h
echo \#define ARDUINO_ESP32_GIT_DESC `git describe --tags 2>/dev/null` >>$outdir/cores/esp32/core_version.h
echo \#define ARDUINO_ESP32_RELEASE_$ver_define >>$outdir/cores/esp32/core_version.h
echo \#define ARDUINO_ESP32_RELEASE \"$ver_define\" >>$outdir/cores/esp32/core_version.h

# Store submodules' current versions
echo " - getting submodule list (${releaseDir}/submodules.txt)..."
git submodule status > $releaseDir/submodules.txt

# remove all .git* files
echo " - removing *.git files possibly fetched to package tempdir..."
find $outdir -name '*.git*' -type f -delete
 
# Zip the package
package_name_zip=$package_name.zip
echo " - creating package ZIP archive (${package_name_zip})..."

pushd $releaseDir >/dev/null

zip -qr $package_name_zip $package_name
if [ $? -ne 0 ]; then echo "     !error: failed to create ${package_name_zip} (ZIP errno: $?) => aborting"; exit 1; fi

# Calculate SHA sum and size of ZIP archive
sha=`shasum -a 256 $package_name_zip | cut -f 1 -d ' '`
size=`/bin/ls -l $package_name_zip | awk '{print $5}'`	
echo "     ${package_name_zip} creation OK (size: $size, sha2: $sha)"
echo

echo "Making $package_name JSON definition file(s):"

popd >/dev/null

PACKAGE_JSON_DEV="package_esp32_dev_index.json"
PACKAGE_JSON_REL="package_esp32_index.json"

# figure out the package type (release / pre-release)
shopt -s nocasematch
if [[ $TRAVIS_TAG == *-RC* ]]; then
	bIsPrerelease=1
	package_name_json=$PACKAGE_JSON_DEV
	echo " - package type: PRE-RELEASE, JSON def.file: $PACKAGE_JSON_DEV"
else
	bIsPrerelease=0
	package_name_json=$PACKAGE_JSON_REL
	echo " - package type: RELEASE, JSON def.files: $PACKAGE_JSON_REL, $PACKAGE_JSON_DEV"
fi
shopt -u nocasematch

# Cleanup temporary work dir
rm -rf $outdir

# Get all previously released versions
echo " - fetching previous (pre)release versions from GitHub..."

set +e

releasesJson=$releaseDir/releases.json
curl -sH "Authorization: token $curlAuth" https://api.github.com/repos/$TRAVIS_REPO_SLUG/releases > $releasesJson
if [ $? -ne 0 ]; then echo "FAILED: $? => aborting"; exit 1; fi

prev_release=$(jq -e -r '. | map(select(.draft == false and .prerelease == false)) | sort_by(.created_at | - fromdateiso8601) | .[0].tag_name' ${releasesJson})
prev_any_release=$(jq -e -r '. | map(select(.draft == false)) | sort_by(.created_at | - fromdateiso8601)  | .[0].tag_name' ${releasesJson})
prev_pre_release=$(jq -e -r '. | map(select(.draft == false and .prerelease == true)) | sort_by(.created_at | - fromdateiso8601)  | .[0].tag_name' ${releasesJson})

shopt -s nocasematch
if [ "$prev_any_release" == "$TRAVIS_TAG" ]; then
	prev_release=$(jq -e -r '. | map(select(.draft == false and .prerelease == false)) | sort_by(.created_at | - fromdateiso8601) | .[1].tag_name' ${releasesJson})
	prev_any_release=$(jq -e -r '. | map(select(.draft == false)) | sort_by(.created_at | - fromdateiso8601)  | .[1].tag_name' ${releasesJson})
	prev_pre_release=$(jq -e -r '. | map(select(.draft == false and .prerelease == true)) | sort_by(.created_at | - fromdateiso8601)  | .[1].tag_name' ${releasesJson})
fi
shopt -u nocasematch

set -e

rm -f "$releasesJson"

echo "     previous Release: $prev_release"
echo "     previous Pre-release: $prev_pre_release"
echo "     previous (any)release: $prev_any_release"

# add generated items to JSON package-definition contents
jq_arg=".packages[0].platforms[0].version = \"$ver\" | \
    .packages[0].platforms[0].url = \"$REMOTE_URL/$package_name_zip\" |\
    .packages[0].platforms[0].archiveFileName = \"$package_name_zip\""

jq_arg="$jq_arg |\
	.packages[0].platforms[0].size = \"$size\" |\
	.packages[0].platforms[0].checksum = \"SHA-256:$sha\""

# always get DEV version of JSON (included in both RC/REL)
pkgJsonDev=$releaseDir/$PACKAGE_JSON_DEV
echo " - generating/merging _DEV_ JSON file (${pkgJsonDev})..."

cat $srcdir/package/package_esp32_index.template.json | jq "$jq_arg" > $pkgJsonDev
if [ ! -z "$prev_any_release" ] && [ "$prev_any_release" != "null" ]; then
	downloadAndMergePackageJSON "https://github.com/$TRAVIS_REPO_SLUG/releases/download/${prev_any_release}/${PACKAGE_JSON_DEV}" "${pkgJsonDev}" "${curlAuth}" "$releaseDir"
	
	# Release notes: GIT log comments (prev_any_release, current_release>
	git log --oneline $prev_any_release.. > $releaseDir/commits.txt
fi	

# for RELEASE run update REL JSON as well
if [ $bIsPrerelease -eq 0 ]; then

	pkgJsonRel=$releaseDir/$PACKAGE_JSON_REL
	echo " - generating/merging _REL_ JSON file (${pkgJsonRel})..."

	cat $srcdir/package/package_esp32_index.template.json | jq "$jq_arg" > $pkgJsonRel
	if [ ! -z "$prev_release" ] && [ "$prev_release" != "null" ]; then
		downloadAndMergePackageJSON "https://github.com/$TRAVIS_REPO_SLUG/releases/download/${prev_release}/${PACKAGE_JSON_REL}" "${pkgJsonRel}" "${curlAuth}" "$releaseDir"
		
		# Release notes: GIT log comments (prev_release, current_release>
		git log --oneline $prev_release.. > $releaseDir/commits.txt
	fi
fi

echo
echo "JSON definition file(s) creation OK"

echo
echo "==================================================================="
echo "Package preparation done ('$releaseDir' contents):"
fileset=`ls -1 $releaseDir`
echo -e $fileset

echo
echo "==================================================================="
echo "==================================================================="
echo "'$package_name' ready for publishing, processing completed."
echo "==================================================================="
echo
