cmake_minimum_required(VERSION 3.10)

project(VCOM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT_NAME} src/test.cpp src/VCOM.cpp src/CRC.cpp)