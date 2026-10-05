cmake_minimum_required(VERSION 3.16)
project(mtfind LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(mtfind
    src/main.cpp
    src/search.cpp
    src/pipeline.cpp
)
target_include_directories(mtfind PRIVATE src)
target_link_libraries(mtfind PRIVATE Threads::Threads)

target_compile_options(mtfind PRIVATE -Wall -Wextra -Wpedantic -Werror)