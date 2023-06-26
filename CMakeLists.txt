cmake_minimum_required(VERSION 3.10)
project(simsti)

set(CMAKE_CXX_STANDARD 20)

include($ENV{SIMPLE_CPP}/cmakes/CPM.cmake)
CPMAddPackage("gh:/fmtlib/fmt#0")
CPMAddPackage("gh:/gabime/spdlog#0")

include_directories($ENV{SIMPLE_CPP})

set(Boost_USE_STATIC_LIBS ON)
include_directories($ENV{BOOST_LATEST})
link_directories($ENV{BOOST_LATEST}/lib64-msvc-14.2)

add_definitions(-DFMT_HEADER_ONLY -DSPDLOG_FMT_EXTERNAL -DSPDLOG_FMT_EXTERNAL_HO -D_WIN32_WINNT=0x601)

add_executable(simsti simsti.cpp)
target_link_libraries(simsti PRIVATE fmt spdlog)

add_executable(rtrrecv rtrrecv.cpp)
target_link_libraries(rtrrecv PRIVATE fmt spdlog)

