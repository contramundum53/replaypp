cmake_minimum_required(VERSION 3.21)
project(replay++)

set(CMAKE_CXX_STANDARD 20)

# if toplevel
message(${CMAKE_CURRENT_LIST_DIR})
if(PROJECT_IS_TOP_LEVEL)
    include_directories(include)
    add_executable(test_replay test/test_replay.cpp)
else()
    add_library(replay++ INTERFACE)
    target_include_directories(replay++ PUBLIC include)
endif()