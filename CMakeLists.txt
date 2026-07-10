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
    src/html/Tokenizer.cpp
    src/html/TreeBuilder.cpp
    src/css/CSSParser.cpp
    src/js/VM.cpp
    src/render/Painter.cpp
    src/layout/Box.cpp
    src/util/String.cpp
    src/util/Memory.cpp
    src/util/Logging.cpp
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

# Windows subsystem (GUI app).  Omit this flag on Linux/macOS so that stdout works.
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# ── Tests ───────────────────────────────────────────────────────────────────
# DOM unit test: exercises the tokeniser, tree builder, DOM node types, and
# innerHTML setter.  Builds cross-platform (no Win32 dependencies).
enable_testing()

add_executable(DOMTest
    tests/DOMTest.cpp
    src/html/DOMNode.cpp
    src/html/Tokenizer.cpp
    src/html/HTMLParser.cpp
    src/html/TreeBuilder.cpp
)
target_include_directories(DOMTest PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(DOMTest PRIVATE TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests")
add_test(NAME DOMTest COMMAND DOMTest)
