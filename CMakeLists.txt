﻿cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)

project(simsti)

include_directories($ENV{SIMPLE_CPP})
include_directories($ENV{VCPKG_ROOT}/installed/$ENV{VCPKG_DEFAULT_TRIPLET}/include)

set(Boost_USE_STATIC_LIBS ON)
include_directories($ENV{BOOST_LATEST})
link_directories($ENV{BOOST_LATEST}/lib64-msvc-14.2)

add_definitions(-DFMT_HEADER_ONLY -D_WIN32_WINNT=0x601)

find_package(Threads)

add_executable(simsti simsti.cpp)
target_link_libraries(simsti PRIVATE Threads::Threads)

add_executable(rtrrecv rtrrecv.cpp)
target_link_libraries(rtrrecv PRIVATE Threads::Threads)

