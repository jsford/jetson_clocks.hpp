cmake_minimum_required(VERSION 3.1)

project(jetson_clocks)

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/jetson_clocks.hpp)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
add_library(JetsonClocks::JetsonClocks ALIAS ${PROJECT_NAME})

add_executable(${PROJECT_NAME}_example example.cpp)
