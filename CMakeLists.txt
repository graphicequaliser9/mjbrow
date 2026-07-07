cmake_minimum_required(VERSION 3.10)
project(NitrogenBrowser VERSION 0.1.0 LANGUAGES CXX)

# ── C++ standard ──────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Compiler flags ────────────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /permissive- /bigobj)
elseif(MINGW)
    add_compile_options(-Wall -Wextra -Wpedantic)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ── Include directories ───────────────────────────────────────────────────────
include_directories(${CMAKE_SOURCE_DIR}/include)

# ── Sources ───────────────────────────────────────────────────────────────────
#
# Cross-platform sources (compile everywhere)
set(CROSS_PLATFORM_SOURCES
    src/main.cpp
    src/net/HttpClient.cpp
    src/html/HTMLParser.cpp
    src/html/DOMNode.cpp
    src/css/CSSParser.cpp
    src/css/StringPool.cpp
    src/css/Cascade.cpp
    src/js/VM.cpp
    src/render/Painter.cpp
    src/layout/Box.cpp
    src/util/String.cpp
    src/util/Memory.cpp
    src/util/Logging.cpp
    src/util/Arena.cpp
    src/browser/Tab.cpp
    src/browser/Bookmarks.cpp
    src/browser/URLBar.cpp
    src/browser/SettingsPanel.cpp
    src/browser/BrowserUI.cpp
    src/devtools/DOMInspector.cpp
    src/devtools/PaintProfiler.cpp
)

# Windows-only sources (guarded with #ifdef _WIN32 in headers / stubs)
set(WINDOWS_SOURCES
    src/core/Win32Window.cpp
)

# Assemble per-platform source lists
set(NITROGEN_SOURCES ${CROSS_PLATFORM_SOURCES})

if(WIN32)
    list(APPEND NITROGEN_SOURCES ${WINDOWS_SOURCES})
endif()

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME} ${NITROGEN_SOURCES})

# ── Table layout benchmark (Bead 5) ────────────────────────────────────────────
# Cross-platform, no Win32 dependency: exercises the virtual-scroll table engine
# and asserts the 3500x250 table uses < 5 MB of layout memory.
set(TABLE_BENCH_SOURCES
    bench/TableLayoutBenchmark.cpp
    src/layout/Box.cpp
    src/html/DOMNode.cpp
    src/html/HTMLParser.cpp
    src/util/Arena.cpp
    src/util/String.cpp
    src/util/Logging.cpp
)
add_executable(nitrogen_table_bench ${TABLE_BENCH_SOURCES})

# ── Painter unit test (Bead 6) ─────────────────────────────────────────────────
# Cross-platform: exercises the platform-independent glyph-cache + text-batching
# logic (no Win32 dependency).  On Windows the same Painter.cpp also provides the
# GDI+ Win32Painter used by the main executable.
set(PAINTER_TEST_SOURCES
    bench/PainterTest.cpp
    src/render/Painter.cpp
)
add_executable(nitrogen_painter_test ${PAINTER_TEST_SOURCES})

# Windows subsystem (GUI app).  Omit this flag on Linux/macOS so that stdout works.
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    # GDI+ (compositing back-end for the Win32Painter).
    target_link_libraries(${PROJECT_NAME} gdiplus)
    target_link_libraries(nitrogen_painter_test gdiplus)
endif()

# ── Tests ──────────────────────────────────────────────────────────────────────
enable_testing()

add_test(NAME nitrogen_table_bench COMMAND nitrogen_table_bench)
add_test(NAME nitrogen_painter_test COMMAND nitrogen_painter_test)

# ── Unit tests for highest-risk modules ────────────────────────────────────────
set(ARENA_TEST_SOURCES
    test/ArenaTest.cpp
    src/util/Arena.cpp
)
add_executable(nitrogen_arena_test ${ARENA_TEST_SOURCES})

set(CSSPARSER_TEST_SOURCES
    test/CSSParserTest.cpp
    src/css/CSSParser.cpp
    src/css/StringPool.cpp
    src/util/String.cpp
    src/util/Logging.cpp
)
add_executable(nitrogen_cssparser_test ${CSSPARSER_TEST_SOURCES})

set(CASCADE_TEST_SOURCES
    test/CascadeTest.cpp
    src/css/CSSParser.cpp
    src/css/StringPool.cpp
    src/css/Cascade.cpp
    src/html/DOMNode.cpp
    src/html/HTMLParser.cpp
    src/util/Arena.cpp
    src/util/String.cpp
    src/util/Logging.cpp
)
add_executable(nitrogen_cascade_test ${CASCADE_TEST_SOURCES})

set(HTMLPARSER_TEST_SOURCES
    test/HTMLParserTest.cpp
    src/html/DOMNode.cpp
    src/html/HTMLParser.cpp
    src/util/Arena.cpp
    src/util/String.cpp
    src/util/Logging.cpp
)
add_executable(nitrogen_htmlparser_test ${HTMLPARSER_TEST_SOURCES})

set(DOMNODEPOOL_TEST_SOURCES
    test/DOMNodePoolTest.cpp
    src/html/DOMNode.cpp
    src/html/HTMLParser.cpp
    src/util/Arena.cpp
    src/util/String.cpp
    src/util/Logging.cpp
)
add_executable(nitrogen_domnode_test ${DOMNODEPOOL_TEST_SOURCES})

add_test(NAME nitrogen_arena_test COMMAND nitrogen_arena_test)
add_test(NAME nitrogen_cssparser_test COMMAND nitrogen_cssparser_test)
add_test(NAME nitrogen_cascade_test COMMAND nitrogen_cascade_test)
add_test(NAME nitrogen_htmlparser_test COMMAND nitrogen_htmlparser_test)
add_test(NAME nitrogen_domnode_test COMMAND nitrogen_domnode_test)

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
