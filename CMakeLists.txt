cmake_minimum_required(VERSION 3.5)

project(event_system LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(event_system
    src/event_manager.h
    src/main.cpp
)
