cmake_minimum_required(VERSION 3.1)

project(jetson_clocks)

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME} INTERFACE jetson_clocks.hpp)

add_executable(${PROJECT_NAME}_example example.cpp)
