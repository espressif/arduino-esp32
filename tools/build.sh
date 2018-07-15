#!/bin/bash

if [ ! -z "$TRAVIS_TAG" ]; then
    # zip the package if tagged build
    tools/build-release.sh -a$ESP32_GITHUB_TOKEN
else
    # run cmake and sketch tests
    tools/check_cmakelists.sh && tools/build-tests.sh
fi
