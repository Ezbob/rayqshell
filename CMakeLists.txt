CMAKE_MINIMUM_REQUIRED(VERSION 3.24)

project(rayqconsole C)

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
# rayqconsole library
# ======================

add_library(rayqconsole STATIC "")

target_sources(rayqconsole
  PRIVATE
    "console.c"
    "console_args.c"
    "commands/core_commands.c"
    "commands/fs_commands.c"
)

target_link_libraries(rayqconsole PRIVATE raylib)

target_include_directories(rayqconsole PUBLIC "commands")

# ======================
# console test
# ======================

add_executable(console_test "")

target_sources(console_test PRIVATE "main.c")

target_link_libraries(console_test PRIVATE raylib rayqconsole)