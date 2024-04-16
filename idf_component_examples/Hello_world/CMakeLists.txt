cmake_minimum_required(VERSION 3.17)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hello_world)

# Download BufferUtils library
include(FetchContent)
FetchContent_Populate(BufferUtils
    URL https://github.com/bakercp/BufferUtils/archive/refs/tags/3.0.0.zip
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/arduino_libs/BufferUtils
)

# Add Arduino libraries to the project
add_arduino_libraries(${CMAKE_CURRENT_LIST_DIR}/arduino_libs)
