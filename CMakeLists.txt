cmake_minimum_required(VERSION 4.2)
project(cpptgc)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(.)

add_executable(cpptgc
        cpptgc.cpp
        cpptgc.h
        table.cpp
        table.h)
