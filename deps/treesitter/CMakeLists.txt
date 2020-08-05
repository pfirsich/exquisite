cmake_minimum_required(VERSION 3.0.0)

project(treesitter)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_BUILD_TYPE Release) # tree-sitter is WAY slower in debug

set(SRC
  tree-sitter/lib/src/lib.c
  tree-sitter-cpp/src/parser.c
  tree-sitter-cpp/src/scanner.cc
)

add_library(treesitter ${SRC})
target_include_directories(treesitter PUBLIC tree-sitter/lib/src)
target_include_directories(treesitter PUBLIC tree-sitter/lib/include)
