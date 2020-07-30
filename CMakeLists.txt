cmake_minimum_required(VERSION 3.0.0)

project(exquisite)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include(cmake/wall.cmake)

if (EXQUISITE_ENABLE_ASAN)
  include(cmake/asan.cmake)
endif()

set(SRC
  buffer.cpp
  control.cpp
  editor.cpp
  main.cpp
  key.cpp
  terminal.cpp
  utf8.cpp
  util.cpp
)
list(TRANSFORM SRC PREPEND src/)

add_executable(exquisite ${SRC})
target_include_directories(exquisite PUBLIC include)
set_wall(exquisite)
