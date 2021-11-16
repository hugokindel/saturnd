cmake_minimum_required(VERSION 3.20)
project(task_planner C)

set(CMAKE_C_STANDARD 99)

add_executable(cassini
        include/sy5/client-request.h
        include/sy5/server-reply.h
        include/sy5/timing.h
        include/sy5/timing-text-io.h
        include/sy5/utils.h
        src/main.c
        src/timing-text-io.c
        src/utils.c)
target_include_directories(cassini PRIVATE include)
target_compile_definitions(cassini PRIVATE CASSINI)

add_executable(saturnd
        include/sy5/client-request.h
        include/sy5/server-reply.h
        include/sy5/timing.h
        include/sy5/timing-text-io.h
        include/sy5/utils.h
        src/main.c
        src/timing-text-io.c
        src/utils.c)
target_include_directories(saturnd PRIVATE include)
target_compile_definitions(saturnd PRIVATE SATURND)