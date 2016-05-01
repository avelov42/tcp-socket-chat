cmake_minimum_required(VERSION 3.3)
project(sik_czat)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_library(err err.cpp)

add_executable(server server.cpp)
add_executable(client client.cpp)

target_link_libraries(server err)
