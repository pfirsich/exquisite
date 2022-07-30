cmake_minimum_required(VERSION 3.0.0)

project(exquisite)

# For conan
list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include(cmake/wall.cmake)

if(EXQUISITE_ENABLE_ASAN)
  include(cmake/asan.cmake)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(PLATFORM "linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(PLATFORM "mac")
else()
  message(FATAL_ERROR "Building is not supported on this platform")
endif()

set(SRC
  src/buffer.cpp
  src/clipboard.cpp
  src/colorscheme.cpp
  src/commands.cpp
  src/commands/find.cpp
  src/config.cpp
  src/control.cpp
  src/editor.cpp
  src/eventhandler.cpp
  src/eventhandler_${PLATFORM}.cpp
  src/fd.cpp
  src/fuzzy.cpp
  src/highlighting.cpp
  src/key.cpp
  src/languages.cpp
  src/languages/cpp.cpp
  src/main.cpp
  src/palette.cpp
  src/process.cpp
  src/terminal.cpp
  src/textbuffer.cpp
  src/tree-sitter.cpp
  src/utf8.cpp
  src/util.cpp
)

# list(TRANSFORM SRC PREPEND src/) CMake on Ubuntu 18.04 can't do this
find_package(fmt)

add_subdirectory(deps/treesitter)

add_executable(exquisite ${SRC})
target_compile_options(exquisite PRIVATE "-fdiagnostics-color=always")
target_include_directories(exquisite PRIVATE src)
target_link_libraries(exquisite stdc++fs) # Ubuntu 18.04 needs this
target_link_libraries(exquisite treesitter)
target_link_libraries(exquisite fmt::fmt)
target_include_directories(exquisite PRIVATE deps/clipp)
set_wall(exquisite)
