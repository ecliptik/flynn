cmake_minimum_required(VERSION 3.9)
project(Flynn)

add_application(Flynn
    src/main.c
    src/connection.c
    src/telnet.c
    src/terminal.c
    src/tcp.c
    src/dnr.c
    src/sysutil.c
    resources/telnet.r
)

# Include path for MacTCP.h and local headers
target_include_directories(Flynn PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Target 68000 CPU (Mac Plus compatible)
set_target_properties(Flynn PROPERTIES
    COMPILE_FLAGS "-m68000"
)

# Optimize for size
set_target_properties(Flynn PROPERTIES
    COMPILE_OPTIONS -ffunction-sections
)
if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    set_target_properties(Flynn PROPERTIES
        LINK_FLAGS "-Wl,-gc-sections"
    )
else()
    set_target_properties(Flynn PROPERTIES
        LINK_FLAGS "-Wl,-gc-sections"
    )
endif()
