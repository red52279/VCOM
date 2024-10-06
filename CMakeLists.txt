cmake_minimum_required(VERSION 3.10)

project(VCOM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(
	include/vcom/include
)

add_subdirectory(include/vcom)

add_executable(${PROJECT_NAME} src/test.cpp)

target_link_directories(${PROJECT_NAME} PRIVATE
	include/vcom/include
)

target_link_libraries(${PROJECT_NAME} PUBLIC
	vcom
)