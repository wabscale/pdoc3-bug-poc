cmake_minimum_required(VERSION 3.15)

project(test VERSION 1.0 LANGUAGES CXX)


add_subdirectory(pybind11-2.11.1)
pybind11_add_module(example example.cpp)

target_compile_definitions(example PRIVATE PYBIND)

