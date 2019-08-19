#!/bin/bash

if [ ! -z "$TRAVIS_TAG" ]; then
	echo "No sketch builds & tests required for tagged TravisCI builds, exiting"
	exit 0
fi

echo -e "travis_fold:start:sketch_test_env_prepare"
tools/prep-arduino-ide.sh
if [ $? -ne 0 ]; then exit 1; fi
echo -e "travis_fold:end:sketch_test_env_prepare"

echo -e "travis_fold:start:platformio_test_env_prepare"
tools/prep-platformio.sh
if [ $? -ne 0 ]; then exit 1; fi
echo -e "travis_fold:end:platformio_test_env_prepare"

cd $TRAVIS_BUILD_DIR

echo -e "travis_fold:start:sketch_test"
tools/test-arduino-ide.sh
if [ $? -ne 0 ]; then exit 1; fi
echo -e "travis_fold:end:sketch_test"

echo -e "travis_fold:start:size_report"
cat size.log
echo -e "travis_fold:end:size_report"

echo -e "travis_fold:start:platformio_test"
tools/test-platformio.sh
if [ $? -ne 0 ]; then exit 1; fi
echo -e "travis_fold:end:platformio_test"
