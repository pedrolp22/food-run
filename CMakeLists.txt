cmake_minimum_required(VERSION 4.2)
project(subway)

set(CMAKE_CXX_STANDARD 20)

add_executable(subway
               main.cpp
        src/Game.cpp
        include/Game.h
        include/Game.h
        src/Renderer.cpp
        include/Renderer.h)

target_include_directories(subway
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external)