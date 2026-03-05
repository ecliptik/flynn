cmake_minimum_required(VERSION 3.9)
project(TelnetM68K)

add_application(TelnetM68K
    src/main.c
    resources/telnet.r
)

# Target 68000 CPU (Mac Plus compatible)
set_target_properties(TelnetM68K PROPERTIES
    COMPILE_FLAGS "-m68000"
)

# Optimize for size
set_target_properties(TelnetM68K PROPERTIES
    COMPILE_OPTIONS -ffunction-sections
)
if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    set_target_properties(TelnetM68K PROPERTIES
        LINK_FLAGS "-Wl,-gc-sections"
    )
else()
    set_target_properties(TelnetM68K PROPERTIES
        LINK_FLAGS "-Wl,-gc-sections"
    )
endif()
