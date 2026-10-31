cmake_minimum_required(VERSION 3.24)
project(big-o-demo-sorting LANGUAGES CXX)
# Unoptimized timings are meaningless, and these demos exist to be timed.
# Nothing here set a build type, so a bare `cmake -B build` was producing a
# debug-ish build and numbers nobody should draw a conclusion from.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_executable(${PROJECT_NAME} src/main.cpp)
add_executable(${PROJECT_NAME}-space src/space_main.cpp)
add_executable(${PROJECT_NAME}-minmax src/minmax_main.cpp)

# Pass repo root so executables can find graph scripts and write CSV there
target_compile_definitions(${PROJECT_NAME} PRIVATE
    REPO_DIR="${CMAKE_SOURCE_DIR}"
)
target_compile_definitions(${PROJECT_NAME}-space PRIVATE
    REPO_DIR="${CMAKE_SOURCE_DIR}"
)
target_compile_definitions(${PROJECT_NAME}-minmax PRIVATE
    REPO_DIR="${CMAKE_SOURCE_DIR}"
)
