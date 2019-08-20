#!/bin/bash

# CMake Test
echo -e "travis_fold:start:check_cmakelists"
tools/check_cmakelists.sh
if [ $? -ne 0 ]; then exit 1; fi
echo -e "travis_fold:end:check_cmakelists"

# ArduinoIDE Test
echo -e "travis_fold:start:prep_arduino_ide"
tools/prep-arduino-ide.sh
if [ $? -ne 0 ]; then exit 1; fi
echo -e "travis_fold:end:prep_arduino_ide"

echo -e "travis_fold:start:test_arduino_ide"
tools/test-arduino-ide.sh
if [ $? -ne 0 ]; then exit 1; fi
echo -e "travis_fold:end:test_arduino_ide"

echo -e "travis_fold:start:size_report"
cat size.log
echo -e "travis_fold:end:size_report"

# PlatformIO Test
echo -e "travis_fold:start:prep_platformio"
tools/prep-platformio.sh
if [ $? -ne 0 ]; then exit 1; fi
echo -e "travis_fold:end:prep_platformio"

echo -e "travis_fold:start:test_platformio"
tools/test-platformio.sh
if [ $? -ne 0 ]; then exit 1; fi
echo -e "travis_fold:end:test_platformio"
