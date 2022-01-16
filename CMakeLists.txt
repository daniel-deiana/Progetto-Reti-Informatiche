cmake_minimum_required(VERSION 3.21)
project(Progetto C)

set(CMAKE_C_STANDARD 11)

include_directories(src)

add_executable(Progetto
        src/Device.c)

set (sources
        myHeader.h)