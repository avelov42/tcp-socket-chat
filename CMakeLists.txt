cmake_minimum_required(VERSION 2.8)
project(sik_czat)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_library(common common.cpp)

add_executable(server server.cpp)
add_executable(client client.cpp)

target_link_libraries(server common)
target_link_libraries(client common)
