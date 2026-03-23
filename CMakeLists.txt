cmake_minimum_required(VERSION 3.25)

project(parking_master_webasm LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

if(EMSCRIPTEN)
  set(PLATFORM Web CACHE STRING "" FORCE)
endif()

set(WEB_SHELL_FILE "${CMAKE_SOURCE_DIR}/web/shell.html" CACHE STRING "Shell template for Emscripten builds")

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(USE_EXTERNAL_GLFW OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
)

FetchContent_MakeAvailable(raylib)

add_executable(parking_master_webasm
  src/main.cpp
)

target_link_libraries(parking_master_webasm PRIVATE raylib)

if(EMSCRIPTEN)
  set_target_properties(parking_master_webasm PROPERTIES
    OUTPUT_NAME "index"
    SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )

  target_link_options(parking_master_webasm PRIVATE
    "-sUSE_GLFW=3"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sMAX_WEBGL_VERSION=2"
    "-sASSERTIONS=1"
    "--shell-file" "${WEB_SHELL_FILE}"
  )
else()
  message(STATUS "Native build supported for local debugging. Use emcmake for the web build.")
endif()
