CMAKE_MINIMUM_REQUIRED(VERSION 3.24)

project(console)

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

add_executable(console_test "")

target_sources(console_test PRIVATE "main.c" "console.c")

target_link_libraries(console_test PRIVATE raylib)