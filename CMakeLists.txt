cmake_minimum_required(VERSION 3.30)
project(aic_key_eamio C)

set(CMAKE_C_STANDARD 23)

file(GLOB_RECURSE sources src/*.c)

find_package(CURL REQUIRED)

add_library(eamio SHARED ${sources})
target_link_libraries(eamio PRIVATE CURL::libcurl setupapi hid)