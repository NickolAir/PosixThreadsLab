cmake_minimum_required(VERSION 3.25)
project(PosixThreadsLab)

set(CMAKE_CXX_STANDARD 17)
include_directories(/usr/include/x86_64-linux-gnu/mpich)
set(CMAKE_C_COMPILER "/usr/bin/mpicc")
set(CMAKE_CXX_COMPILER "/usr/bin/mpic++")

add_executable(PosixThreadsLab PThreads.cpp)
target_link_libraries(PosixThreadsLab m)