cmake_minimum_required(VERSION 3.10)
project(kernel_space)

set(CMAKE_CXX_STANDARD 17)

add_executable(kernek_space
              main.cpp
              heap_overflow.cpp
              kernel_access.cpp 
              )
