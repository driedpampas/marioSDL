cmake_minimum_required(VERSION 3.29)
project(marioSDL)

set(CMAKE_CXX_STANDARD 23)

include_directories(${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS})

add_executable(marioSDL main.cpp
        functions.cpp
        functions.h)
target_link_libraries(marioSDL SDL2 SDL2_image)