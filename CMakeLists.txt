cmake_minimum_required(VERSION 3.9)
project(Flynn VERSION 1.9.5)

# --- Feature flags with defaults ---
option(FLYNN_FINGER        "Enable Finger protocol"            ON)
option(FLYNN_COLOR         "Enable 256-color support"          OFF)
option(FLYNN_GLYPHS        "Enable Unicode glyph rendering"    ON)
option(FLYNN_CP437         "Enable CP437 character set"         ON)
option(FLYNN_CLIPBOARD     "Enable clipboard copy/paste"        ON)
option(FLYNN_SAVEFILE      "Enable save session to file"        ON)
option(FLYNN_BOOKMARKS     "Enable favorites/bookmarks"         ON)
option(FLYNN_DARK_MODE     "Enable dark mode toggle"            ON)
option(FLYNN_DBLWIDTH      "Enable double-width/height lines"   OFF)
option(FLYNN_ALT_SCREEN    "Enable alternate screen buffer"     ON)
option(FLYNN_OFFSCREEN     "Enable offscreen double-buffer"     ON)
option(FLYNN_STATUS_BAR    "Enable status bar"                  ON)
option(FLYNN_CURSOR_STYLES "Enable DECSCUSR cursor styles"      ON)
option(FLYNN_TAB_STOPS     "Enable HTS/TBC custom tab stops"    ON)

# Numeric flags
if(NOT DEFINED FLYNN_MAX_SESSIONS)
    set(FLYNN_MAX_SESSIONS 1)
endif()
if(NOT DEFINED FLYNN_SCROLLBACK)
    set(FLYNN_SCROLLBACK 96)
endif()

# --- Conditional source files ---
set(FLYNN_SOURCES
    src/main.c
    src/menus.c
    src/input.c
    src/dialogs.c
    src/session.c
    src/connection.c
    src/telnet.c
    src/terminal.c
    src/terminal_ui.c
    src/tcp.c
    src/dns.c
    src/macutil.c
    src/sysutil.c
    src/settings.c
)
if(FLYNN_FINGER)
    list(APPEND FLYNN_SOURCES src/finger.c)
endif()
if(FLYNN_GLYPHS)
    list(APPEND FLYNN_SOURCES src/glyphs.c)
endif()
if(FLYNN_CP437)
    list(APPEND FLYNN_SOURCES src/cp437.c)
endif()
if(FLYNN_CLIPBOARD)
    list(APPEND FLYNN_SOURCES src/clipboard.c)
endif()
if(FLYNN_SAVEFILE)
    list(APPEND FLYNN_SOURCES src/savefile.c)
endif()
if(FLYNN_COLOR)
    list(APPEND FLYNN_SOURCES src/color.c)
endif()

add_application(Flynn ${FLYNN_SOURCES} resources/telnet.r)

# Include path for MacTCP.h and local headers
target_include_directories(Flynn PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# --- Pass all flags to C code as compile definitions ---
target_compile_definitions(Flynn PRIVATE
    MAX_SESSIONS=${FLYNN_MAX_SESSIONS}
    FLYNN_SCROLLBACK_LINES=${FLYNN_SCROLLBACK}
    $<$<BOOL:${FLYNN_FINGER}>:FLYNN_FINGER>
    $<$<BOOL:${FLYNN_COLOR}>:FLYNN_COLOR>
    $<$<BOOL:${FLYNN_GLYPHS}>:FLYNN_GLYPHS>
    $<$<BOOL:${FLYNN_CP437}>:FLYNN_CP437>
    $<$<BOOL:${FLYNN_CLIPBOARD}>:FLYNN_CLIPBOARD>
    $<$<BOOL:${FLYNN_SAVEFILE}>:FLYNN_SAVEFILE>
    $<$<BOOL:${FLYNN_BOOKMARKS}>:FLYNN_BOOKMARKS>
    $<$<BOOL:${FLYNN_DARK_MODE}>:FLYNN_DARK_MODE>
    $<$<BOOL:${FLYNN_DBLWIDTH}>:FLYNN_DBLWIDTH>
    $<$<BOOL:${FLYNN_ALT_SCREEN}>:FLYNN_ALT_SCREEN>
    $<$<BOOL:${FLYNN_OFFSCREEN}>:FLYNN_OFFSCREEN>
    $<$<BOOL:${FLYNN_STATUS_BAR}>:FLYNN_STATUS_BAR>
    $<$<BOOL:${FLYNN_CURSOR_STYLES}>:FLYNN_CURSOR_STYLES>
    $<$<BOOL:${FLYNN_TAB_STOPS}>:FLYNN_TAB_STOPS>
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
