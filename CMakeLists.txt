cmake_minimum_required(VERSION 3.25)

project(dobong_driving_test_simulator LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

if(EMSCRIPTEN)
  set(PLATFORM Web CACHE STRING "" FORCE)
endif()

set(WEB_SHELL_FILE "${CMAKE_SOURCE_DIR}/web/shell.html" CACHE STRING "Shell template for Emscripten builds")
set(WEB_OUTPUT_DIR "${CMAKE_SOURCE_DIR}" CACHE PATH "Directory for generated web bundle")

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

add_executable(dobong_driving_test_simulator
  src/main.cpp
)

target_link_libraries(dobong_driving_test_simulator PRIVATE raylib)

if(EMSCRIPTEN)
  set_target_properties(dobong_driving_test_simulator PROPERTIES
    OUTPUT_NAME "index"
    SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY "${WEB_OUTPUT_DIR}"
  )

  target_link_options(dobong_driving_test_simulator PRIVATE
    "-sUSE_GLFW=3"
    "-sALLOW_MEMORY_GROWTH=0"
    "-sINITIAL_MEMORY=67108864"
    "-sMIN_WEBGL_VERSION=1"
    "-sMAX_WEBGL_VERSION=1"
    "-sASSERTIONS=1"
    "-sGL_PREINITIALIZED_CONTEXT=1"
    "-sINCOMING_MODULE_JS_API=canvas,preinitializedWebGLContext,locateFile,onAbort,setStatus,monitorRunDependencies"
    "--shell-file" "${WEB_SHELL_FILE}"
  )
else()
  message(STATUS "Native build supported for local debugging. Use emcmake for the web build.")
endif()
