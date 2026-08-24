cmake_minimum_required(VERSION 4.2)
project(subway)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

add_library(glad STATIC
        external/glad/glad.c)

target_include_directories(glad
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external)

add_executable(subway
        main.cpp
        src/Game.cpp
        include/Game.h
        src/Renderer.cpp
        include/Renderer.h)

target_include_directories(subway
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(subway
        PRIVATE glad)

if (TARGET SDL2::SDL2)
    target_link_libraries(subway
            PRIVATE SDL2::SDL2)
else()
    target_include_directories(subway
            PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(subway
            PRIVATE ${SDL2_LIBRARIES})
endif()
