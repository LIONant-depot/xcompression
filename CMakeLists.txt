cmake_minimum_required(VERSION 3.10)
project(xcompression_unit_test LANGUAGES CXX)
set(TARGET_PROJECT "xcompression_unit_test")
set(COMPONENT_REGISTRY "")

# Fetch Common.cmake from xcmake_tools
include(FetchContent)
FetchContent_Declare(
  xcmake_tools
  GIT_REPOSITORY https://github.com/LIONant-depot/xcmake_tools.git
  GIT_TAG main
  GIT_SHALLOW TRUE
  SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/xcmake_tools"
)
FetchContent_MakeAvailable(xcmake_tools)
include(${CMAKE_BINARY_DIR}/_deps/xcmake_tools/Common.cmake)

# Define the main executable
add_executable(${TARGET_PROJECT}
  "source/unit-test/main.cpp"
  "source/unit-test/xcompression_unittest.h"
)

# Organize source files in IDE
source_group("unit_test" FILES
  "source/unit-test/xcompression_unittest.h"
)
source_group("" FILES
  "source/unit-test/main.cpp"
)

# Add dependency subdirectory
add_subdirectory("build/dependency" "${CMAKE_CURRENT_BINARY_DIR}/xcompression")

# Process components
ProcessComponents()