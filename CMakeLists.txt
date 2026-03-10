cmake_minimum_required(VERSION 3.9)
project(Flynn VERSION 1.8.1)

add_application(Flynn
    src/main.c
    src/menus.c
    src/input.c
    src/clipboard.c
    src/savefile.c
    src/dialogs.c
    src/session.c
    src/connection.c
    src/telnet.c
    src/terminal.c
    src/terminal_ui.c
    src/tcp.c
    src/dns.c
    src/glyphs.c
    src/macutil.c
    src/sysutil.c
    src/settings.c
    resources/telnet.r
)

# Include path for MacTCP.h and local headers
target_include_directories(Flynn PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Target 68000 CPU (Mac Plus compatible) and optimize for size
# -m68000: explicit 68000 targeting (documents intent, implicit in toolchain)
# -Os: optimize for size (critical for 68000's 16-bit bus, no cache)
# -ffunction-sections/-fdata-sections: enable dead code/data elimination
target_compile_options(Flynn PRIVATE
    -m68000
    -Os
    -ffunction-sections
    -fdata-sections
)

# Link-time dead code/data elimination via --gc-sections
set_target_properties(Flynn PROPERTIES
    LINK_FLAGS "-Wl,-gc-sections"
)
