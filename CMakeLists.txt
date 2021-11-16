cmake_minimum_required(VERSION 3.20)
project(saturnd C)

set(CMAKE_C_STANDARD 99)

add_executable(saturnd
        include/cassini.h
        include/client-request.h
        include/server-reply.h
        include/timing.h
        include/timing-text-io.h
        src/cassini.c
        src/timing-text-io.c)
target_include_directories(saturnd PRIVATE include)