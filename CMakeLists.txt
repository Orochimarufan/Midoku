cmake_minimum_required(VERSION 3.14)

project(Midoku LANGUAGES CXX)

# Settings
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set(CMAKE_CXX_FLAGS_DEBUG "-fsanitize=address -fno-omit-frame-pointer")
#set(CMAKE_EXE_LINKER_FLAGS_DEBUG -fsanitize=address)

# Dependencies
include(FindPkgConfig)

find_package(Qt5 COMPONENTS Core Quick Sql Widgets DBus REQUIRED)
pkg_check_modules(TagLib REQUIRED IMPORTED_TARGET taglib)

# Components
add_subdirectory(src)
