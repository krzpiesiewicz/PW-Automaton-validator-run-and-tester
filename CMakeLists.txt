cmake_minimum_required(VERSION 3.8)
project(PW-Automat C)

#set(CMAKE_C_STANDARD ...)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -g -DDEBUG -Wextra")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

# http://stackoverflow.com/questions/10555706/
macro(add_executable _name)
    # invoke built-in add_executable
    _add_executable(${ARGV})
    if (TARGET ${_name})
        target_link_libraries(${_name} rt m)
        add_dependencies(${_name} err msg states)

    endif ()
endmacro()

add_custom_target(err
        ALL
        SOURCES err.c err.h
        )

add_custom_target(msg
        ALL
        SOURCES msg.c msg.h err.h err.c
        )

add_custom_target(states
        ALL
        SOURCES states.c states.h err.c err.h
        )

add_executable(validator validator.c msg.c err.c msg.h err.h)
add_executable(tester tester.c msg.c err.c msg.h err.h)
add_executable(run run.c states.c msg.c err.c states.h msg.h err.h)

install(TARGETS DESTINATION .)
