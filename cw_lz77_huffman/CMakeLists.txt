cmake_minimum_required(VERSION 3.15)
project(archiver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(archiver
    main.cpp
    src/lz77.cpp
    src/utils.cpp
    src/archiver.cpp
    src/huffman.cpp
)
