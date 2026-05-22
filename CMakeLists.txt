cmake_minimum_required(VERSION 3.10)
project(NitrogenBrowser VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Detect compiler and set appropriate flags
if(MSVC)
    # MSVC specific flags
    add_compile_options(/W4 /permissive- /bigobj)
elseif(MINGW)
    # MinGW specific flags
    add_compile_options(-Wall -Wextra)
else()
    # GCC/Clang
    add_compile_options(-Wall -Wextra)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/js/Value.cpp
    src/js/Heap.cpp
    src/js/GC.cpp
    src/js/Parser.cpp
    src/js/BytecodeCompiler.cpp
    src/js/VM.cpp
    src/js/Promise.cpp
    src/js/JSContext.cpp
    src/js/DOMBindings.cpp
)

# Include directories
include_directories(include)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Set target properties for Windows subsystem (GUI application)
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# Install rules (optional)
install(TARGETS ${PROJECT_NAME} DESTINATION bin)