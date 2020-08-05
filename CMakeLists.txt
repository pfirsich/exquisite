cmake_minimum_required(VERSION 3.0.0)

project(exquisite)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include(cmake/wall.cmake)

if (EXQUISITE_ENABLE_ASAN)
  include(cmake/asan.cmake)
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

add_subdirectory(deps/treesitter)

add_executable(exquisite ${SRC})
target_include_directories(exquisite PUBLIC src)
target_link_libraries(exquisite stdc++fs) # Ubuntu 18.04 needs this
target_link_libraries(exquisite treesitter)
set_wall(exquisite)
