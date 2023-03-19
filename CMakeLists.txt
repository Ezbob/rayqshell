CMAKE_MINIMUM_REQUIRED(VERSION 3.24)

project(rayqshell C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ======================
# FetchContent dependencies
# ======================

include(FetchContent)

FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG        4.2.0
)

FetchContent_MakeAvailable(raylib)

if(NOT raylib_POPULATED)
  FetchContent_Populate(raylib)

  add_subdirectory(${raylib_SOURCE_DIR} ${raylib_BINARY_DIR})
endif()

# ======================
# rayqshell library
# ======================

add_library(rayqshell STATIC "")

target_sources(rayqshell
  PRIVATE
    "rqshell.c"
    "rqshell_args.c"
    "commands/core_commands.c"
    "commands/fs_commands.c"
)

target_link_libraries(rayqshell PRIVATE raylib)

target_include_directories(rayqshell PUBLIC "commands")

# ======================
# console test
# ======================

add_executable(console_test EXCLUDE_FROM_ALL "")

target_sources(console_test PRIVATE "main.c")

target_link_libraries(console_test PRIVATE raylib rayqshell)