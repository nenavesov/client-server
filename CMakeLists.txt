cmake_minimum_required(VERSION 3.0)
project(FileClient)

set(CMAKE_CXX_STANDARD 11)

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    include_directories(${CMAKE_SOURCE_DIR})
    add_executable(file_client client.cpp)
    target_link_libraries(file_client ws2_32)
else()
    add_executable(file_client client.cpp)
endif()

