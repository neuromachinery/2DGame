cmake_minimum_required(VERSION 3.10.0)
project(main-x86_64-linux-gnu VERSION 0.1.0 LANGUAGES C)

add_library(cjson STATIC cJSON/cJSON.c)
set(SDLTTF_VENDORED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# This assumes the SDL source is available in vendored/SDL
add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)
add_subdirectory(vendored/SDL3_ttf EXCLUDE_FROM_ALL)
add_subdirectory(vendored/SDL3_image EXCLUDE_FROM_ALL)
# Create your game executable target as usual
add_executable(main-x86_64-linux-gnu WIN32 main.c)

# Link to the actual SDL3 library.
#target_link_libraries(main-x86_64-linux-gnu PRIVATE SDL3::SDL3)
target_link_libraries(main-x86_64-linux-gnu PRIVATE SDL3_ttf::SDL3_ttf cjson m SDL3::SDL3 SDL3_image::SDL3_image)
target_include_directories(main-x86_64-linux-gnu PRIVATE "${PROJECT_SOURCE_DIR}/cJSON" PRIVATE "${PROJECT_SOURCE_DIR}/vendored/SDL3_ttf")
include(CTest)
enable_testing()