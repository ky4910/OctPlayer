cmake_minimum_required(VERSION 3.16)

project(OctPlayer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml OpenGL)
# find_package(GTest REQUIRED)
set(CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/SDL3-3.2.18/x86_64-w64-mingw32)
find_package(SDL3 REQUIRED)

message(STATUS "CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")

# find_package(PkgConfig REQUIRED)

# set(FFMPEG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg60)
# set(SDL2_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/SDL2-2.30.10/x86_64-w64-mingw32)

# include_directories(
#     ${FFMPEG_ROOT}/include
#     ${SDL2_ROOT}/include
# )

# link_directories(
#     ${FFMPEG_ROOT}/lib
#     ${SDL2_ROOT}/lib
# )

# message(STATUS "CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}")

# # pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswresample libswscale)

add_subdirectory(player)
add_subdirectory(app)
# add_subdirectory(tests)
