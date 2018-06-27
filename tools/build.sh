#!/bin/bash

# run cmake tests
tools/check_cmakelists.sh
if [ $? -ne 0 ]; then exit 1; fi

# run sketch tests
tools/build-tests.sh
if [ $? -ne 0 ]; then exit 1; fi

# zip the package if tagged build, otherwise finish here
tools/build-release.sh -a$ESP32_GITHUB_TOKEN
