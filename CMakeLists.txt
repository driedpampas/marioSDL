cmake_minimum_required(VERSION 3.20)
project(marioSDL)

set(CMAKE_CXX_STANDARD 23)

if(WIN32)
    set(SDL2_INCLUDE_DIRS "/home/astra/Downloads/SDL2-2.30.10/x86_64-w64-mingw32/include")
    set(SDL2_IMAGE_INCLUDE_DIRS "/home/astra/Downloads/SDL2-2.30.10/x86_64-w64-mingw32/include")
    set(SDL2_LIBRARIES "/home/astra/Downloads/SDL2-2.30.10/x86_64-w64-mingw32/lib" "/home/astra/Downloads/SDL2-2.30.10/x86_64-w64-mingw32/lib" "/home/astra/Downloads/SDL2-2.30.10/x86_64-w64-mingw32/lib" "/home/astra/Downloads/SDL2-2.30.10/x86_64-w64-mingw32/lib")
else()
    include_directories(${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS})
    set(SDL2_LIBRARIES SDL2 SDL2_image SDL2_mixer SDL2_ttf)
endif()

add_executable(marioSDL main.cpp)
target_link_libraries(marioSDL ${SDL2_LIBRARIES})