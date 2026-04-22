cmake_minimum_required(VERSION 3.20)
project(shapes CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(shapes_classic classic.cpp)
add_executable(shapes_modern modern.cpp)
add_executable(shapes_closures closures.cpp)
