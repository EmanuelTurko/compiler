cmake_minimum_required(VERSION 3.25)

project(compiler_hydro)

set(CMAKE_CXX_STANDARD 20)

add_executable(hydro src/main.cpp
        src/generation.hpp
        src/arena.hpp)
target_compile_options(hydro PRIVATE -Wall -Wextra)
