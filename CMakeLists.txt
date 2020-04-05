cmake_minimum_required(VERSION 2.8)

project(DVPL)
add_executable(${PROJECT_NAME} "main.c")

target_link_libraries(${PROJECT_NAME} "potato")
#"lz4" "z")
