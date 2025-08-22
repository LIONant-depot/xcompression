# The library
cmake_minimum_required(VERSION 3.10)

# Set policy CMP0169 to OLD to allow FetchContent_Populate
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

#
# Populate zstd
#
FetchContent_Declare(
  zstd
  GIT_REPOSITORY https://github.com/facebook/zstd.git
  GIT_TAG        release
  SOURCE_DIR     "${CMAKE_SOURCE_DIR}/dependencies/zstd"
)

FetchContent_GetProperties(zstd)
if(NOT zstd_POPULATED)
  message(STATUS "Populating zstd...")
  FetchContent_Populate(zstd)

  # Create the visual studio solution for the zstd
  execute_process(
      COMMAND ${CMAKE_COMMAND} -B build-cmake -S build/cmake -G "Visual Studio 17 2022" -A x64
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/dependencies/zstd"
      RESULT_VARIABLE result
  )
  if(result)
      message(FATAL_ERROR "CMake generation failed: ${result}")
  endif()

  # compile the release build
  execute_process(
      COMMAND ${CMAKE_COMMAND} --build build-cmake --config Release --target libzstd_static
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/dependencies/zstd"
      RESULT_VARIABLE result
  )
  if(result)
      message(FATAL_ERROR "CMake generation failed: ${result}")
  endif()

  # compile the debug build
  execute_process(
      COMMAND ${CMAKE_COMMAND} --build build-cmake --config Debug --target libzstd_static
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/dependencies/zstd"
      RESULT_VARIABLE result
  )
  if(result)
      message(FATAL_ERROR "CMake generation failed: ${result}")
  endif()

endif()

#
# Populate xerr
#
FetchContent_Declare(
  xerr
  GIT_REPOSITORY https://github.com/LIONant-depot/xerr.git
  GIT_TAG        main
  SOURCE_DIR     "${CMAKE_SOURCE_DIR}/dependencies/xerr"  # Adjusted to go up one level
)

FetchContent_GetProperties(xerr)
if(NOT xerr_POPULATED)
  message(STATUS "Populating xerr...")
  FetchContent_Populate(xerr)
  
  # Include xerr's CMakeLists.txt
  add_subdirectory("${CMAKE_SOURCE_DIR}/dependencies/xerr/build/dependency" "${CMAKE_CURRENT_BINARY_DIR}/xerr")
endif()


#
# Define as an interface library only if not already defined
#
if(NOT TARGET xcompression)

  # Define as a proper static library
  add_library(xcompression INTERFACE)

  # determine the root of our library
  if(DEFINED TARGET_PROJECT AND "${TARGET_PROJECT}" STREQUAL "xcompression_unit_test")
    set(ROOT ".")

    # set the include path
    set(XCOMPRESSION_INCLUDE_LIST "${CMAKE_SOURCE_DIR}/dependencies/zstd"
    )

  else()
    set(ROOT "dependencies/xcompression")

    # set the include path
    set(XCOMPRESSION_INCLUDE_LIST "${ROOT}"
                                  "${CMAKE_SOURCE_DIR}/dependencies/zstd"
    )
  endif()

  # set the files required
  set(XCOMPRESSION_FILES_LIST
    "${ROOT}/source/xcompression.h"
    "${ROOT}/source/xcompression.cpp"
    "${ROOT}/README.md"
  )

  # set linker paths
  set(XCOMPRESSION_LINKER_PATHS
      "${CMAKE_SOURCE_DIR}/dependencies/zstd"
  )

  # Set global properties
  set_property(GLOBAL PROPERTY XCOMPRESSION_GROUP "dependencies/xcore/xcompression")
  set_property(GLOBAL PROPERTY XCOMPRESSION_FILES "${XCOMPRESSION_FILES_LIST}")
  set_property(GLOBAL PROPERTY XCOMPRESSION_INCLUDES "${XCOMPRESSION_INCLUDE_LIST}")
  set_property(GLOBAL PROPERTY XCOMPRESSION_LINKER_PATHS "${XCOMPRESSION_LINKER_PATHS}")
  set_property(GLOBAL APPEND PROPERTY COMPONENT_REGISTRY "XCOMPRESSION")

endif()