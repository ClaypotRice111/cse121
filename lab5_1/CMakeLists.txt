
cmake_minimum_required(VERSION 3.16)

project(lab5_1 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiod)

add_executable(send send.c)

target_link_libraries(send PRIVATE PkgConfig::GPIOD)

target_compile_options(send PRIVATE
    -Wall
    -Wextra
    -O2
)
