cmake_minimum_required (VERSION 2.6)
project (my_server)

file(GLOB SOURCES "src/*.c")
set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-g -O2 -Wall -Wextra -pthread -DNDEBUG")
add_executable(my_server ${SOURCES})

