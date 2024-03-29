cmake_minimum_required(VERSION 3.3)
project(task_planner C)

set(CMAKE_C_STANDARD 99)

if (UNIX AND NOT APPLE)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
endif()

add_executable(cassini
        include/sy5/array.h
        include/sy5/reply.h
        include/sy5/request.h
        include/sy5/types.h
        include/sy5/utils.h
        src/cassini.c
        src/common.c
        src/reply.c
        src/request.c
        src/utils.c)
target_include_directories(cassini PRIVATE include)
target_compile_definitions(cassini PRIVATE CASSINI)

add_executable(saturnd
        include/sy5/array.h
        include/sy5/reply.h
        include/sy5/request.h
        include/sy5/types.h
        include/sy5/utils.h
        src/saturnd.c
        src/worker.c
        src/common.c
        src/reply.c
        src/request.c
        src/utils.c)
target_include_directories(saturnd PRIVATE include)
target_compile_definitions(saturnd PRIVATE SATURND)
target_compile_definitions(saturnd PRIVATE DAEMONIZE)
if (UNIX AND NOT APPLE)
    target_link_libraries(saturnd PRIVATE Threads::Threads)
endif()