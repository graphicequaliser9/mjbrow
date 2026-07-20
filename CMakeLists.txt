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
    src/html/Tokenizer.cpp
    src/html/TreeBuilder.cpp
    src/css/CSSParser.cpp
    src/css/Cascade.cpp
    src/js/VM.cpp
    src/js/QuickJS.cpp
    src/render/Painter.cpp
    src/layout/Box.cpp
    src/layout/TextMeasurer.cpp
    src/layout/BlockLayout.cpp
    src/layout/InlineLayout.cpp
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
    src/html/Tokenizer.cpp
    src/html/HTMLParser.cpp
    src/html/TreeBuilder.cpp
)
target_include_directories(DOMTest PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME DOMTest COMMAND DOMTest)

# ── JS runtime integration test (Bead B) ──────────────────────────────────────
# Exercises the QuickJS embedding + DOM bindings: a page with an inline <script>
# that mutates document.body.innerHTML must update the rendered DOM tree.
add_executable(JSTest
    tests/JSTest.cpp
    src/js/QuickJS.cpp
    src/js/VM.cpp
    src/browser/Tab.cpp
    src/html/Tokenizer.cpp
    src/html/HTMLParser.cpp
    src/html/TreeBuilder.cpp
    src/css/CSSParser.cpp
    src/css/Cascade.cpp
    src/layout/Box.cpp
    src/layout/BlockLayout.cpp
    src/layout/InlineLayout.cpp
    src/layout/TextMeasurer.cpp
    src/net/HttpClient.cpp
    src/util/String.cpp
    src/util/Memory.cpp
    src/util/Logging.cpp
)
target_include_directories(JSTest PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME JSTest COMMAND JSTest)

# ── CSS parser unit test (Bead A) ─────────────────────────────────────────────
# Exercises the tokenizer, rule-set parser (type / class / id / descendant /
# child selectors), declaration parsing, at-rules (@media, @font-face) and
# inline style="" attribute parsing.
add_executable(CSSTest
    tests/CSSTest.cpp
    src/css/CSSParser.cpp
)
target_include_directories(CSSTest PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME CSSTest COMMAND CSSTest)

# ── CSS cascade unit test (Bead B) ────────────────────────────────────────────
# Exercises selector matching (type / class / id / descendant / child),
# specificity ranking, source-order tie-breaking, inline-style override and the
# :visited / :link / :hover pseudo-class no-ops, asserting the final computed
# property map for target elements.
add_executable(CascadeTest
    tests/CascadeTest.cpp
    src/css/CSSParser.cpp
    src/css/Cascade.cpp
    src/html/HTMLParser.cpp
    src/html/Tokenizer.cpp
    src/html/TreeBuilder.cpp
)
target_include_directories(CascadeTest PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME CascadeTest COMMAND CascadeTest)

# ── Style render unit test (Bead C) ────────────────────────────────────────────
# Verifies ComputedStyle values from inline styles, stylesheet rules, and the
# Tab cascade pipeline for the acceptance case.
add_executable(StyleRenderTest
    tests/StyleRenderTest.cpp
    src/css/CSSParser.cpp
    src/css/Cascade.cpp
    src/layout/Box.cpp
    src/layout/BlockLayout.cpp
    src/layout/InlineLayout.cpp
    src/layout/TextMeasurer.cpp
    src/browser/Tab.cpp
    src/html/HTMLParser.cpp
    src/html/Tokenizer.cpp
    src/html/TreeBuilder.cpp
    src/js/QuickJS.cpp
    src/js/VM.cpp
    src/net/HttpClient.cpp
    src/util/String.cpp
    src/util/Memory.cpp
    src/util/Logging.cpp
)
target_include_directories(StyleRenderTest PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME StyleRenderTest COMMAND StyleRenderTest)

# ── Layout engine unit test (Bead A) ────────────────────────────────────────────
# Verifies that the layout engine produces a positioned LayoutNode tree from
# a DOM subtree with block and inline elements.
add_executable(LayoutTest
    tests/LayoutTest.cpp
    src/layout/Box.cpp
    src/layout/BlockLayout.cpp
    src/layout/InlineLayout.cpp
    src/layout/TextMeasurer.cpp
    src/html/HTMLParser.cpp
    src/html/Tokenizer.cpp
    src/html/TreeBuilder.cpp
    src/css/Cascade.cpp
    src/css/CSSParser.cpp
    src/browser/Tab.cpp
    src/js/QuickJS.cpp
    src/js/VM.cpp
    src/net/HttpClient.cpp
    src/util/String.cpp
    src/util/Memory.cpp
    src/util/Logging.cpp
)
target_include_directories(LayoutTest PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME LayoutTest COMMAND LayoutTest)
