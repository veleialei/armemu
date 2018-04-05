cmake_minimum_required(VERSION 3.8)
project(armemu)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        analyze.c
        armemu.c)

add_executable(armemu ${SOURCE_FILES})