include(${CMAKE_BINARY_DIR}/_deps/xcmake_tools/Common.cmake)

# Fetch and build zstd
FetchAndPopulate("https://github.com/facebook/zstd.git" "release")
execute_process(
  COMMAND ${CMAKE_COMMAND} -B build-cmake -S build/cmake -G "Visual Studio 17 2022" -A x64
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/dependencies/zstd"
  RESULT_VARIABLE result
)
if(result)
  message(FATAL_ERROR "zstd CMake generation failed: ${result}")
endif()
execute_process(
  COMMAND ${CMAKE_COMMAND} --build build-cmake --config Release --target libzstd_static
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/dependencies/zstd"
  RESULT_VARIABLE result
)
if(result)
  message(FATAL_ERROR "zstd Release build failed: ${result}")
endif()
execute_process(
  COMMAND ${CMAKE_COMMAND} --build build-cmake --config Debug --target libzstd_static
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/dependencies/zstd"
  RESULT_VARIABLE result
)
if(result)
  message(FATAL_ERROR "zstd Debug build failed: ${result}")
endif()

# Fetch xerr
FetchAndPopulate("https://github.com/LIONant-depot/xerr.git" "main")


DefineInterfaceComponent(xcompression "dependencies/xcore"
  "source/xcompression.h"
  "source/xcompression.cpp"
  "README.md"
)
if(xcompression_CREATED)
   AppendComponentIncludes(xcompression "${CMAKE_SOURCE_DIR}/dependencies/zstd")

   # Set linker paths
   set_property(GLOBAL PROPERTY xcompression_LINKER_PATHS "${CMAKE_SOURCE_DIR}/dependencies/zstd")
endif()

