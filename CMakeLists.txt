cmake_minimum_required(VERSION 3.30)
project(aic_key_eamio C)

set(CMAKE_C_STANDARD 23)

add_library(aic_key_eamio SHARED library.c)
