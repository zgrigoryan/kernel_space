cmake_minimum_required(VERSION 3.10)
project(kernel_space CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(kernel_space
    main.cpp
    heap_overflow.cpp
    kernel_access.cpp
)

if (MSVC)
    target_compile_options(kernel_space PRIVATE /EHa)  
endif()
