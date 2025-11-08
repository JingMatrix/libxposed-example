cmake_minimum_required(VERSION 3.22.1)
project("native")

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(${CMAKE_PROJECT_NAME} SHARED demo.cpp native_api.hpp)

target_link_libraries(${CMAKE_PROJECT_NAME} log)
