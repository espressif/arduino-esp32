# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.17)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Hello_External_Libs)

# Download BufferUtils library
include(FetchContent)
FetchContent_Populate(BufferUtils
    URL https://github.com/bakercp/BufferUtils/archive/refs/tags/3.0.0.zip
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/components/BufferUtils
)

# Convert the Arduino libraries that are missing 'CMakeLists.txt' into IDF components
convert_arduino_libraries_to_components()
