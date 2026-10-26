cmake_minimum_required(VERSION 3.25)

project(compiler_hydro)

set(CMAKE_CXX_STANDARD 20)

add_executable(hydro src/main.cpp)
target_compile_options(hydro PRIVATE -Wall -Wextra)

add_custom_target(run
        COMMAND $<TARGET_FILE:hydro> ${CMAKE_SOURCE_DIR}/test.hy
        COMMAND sh -c "./out; echo exit code: $?"
        DEPENDS hydro
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        USES_TERMINAL
        VERBATIM
)