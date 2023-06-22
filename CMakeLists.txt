cmake_minimum_required(VERSION 3.10)
project(simsti)

set(CMAKE_CXX_STANDARD 20)

include_directories($ENV{SIMPLE_CPP})
set(Boost_USE_STATIC_LIBS ON)
include_directories($ENV{BOOST-LATEST})
link_directories($ENV{BOOST-LATEST}/lib64-msvc-14.2)

add_definitions(-DFMT_HEADER_ONLY -DSPDLOG_FMT_EXTERNAL -DSPDLOG_FMT_EXTERNAL_HO -D_WIN32_WINNT=0x601)

find_package(spdlog CONFIG REQUIRED)

add_executable(simsti simsti.cpp)
target_link_libraries(simsti PRIVATE spdlog::spdlog_header_only)

add_executable(rtrrecv rtrrecv.cpp)
target_link_libraries(rtrrecv PRIVATE spdlog::spdlog_header_only)

