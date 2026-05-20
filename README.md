# Nitrogen - Lightweight Windows Browser

A lightweight web browser for Windows built with modern C++ (C++17) and Win32 API.

## Features
- HTML5 parsing and rendering
- CSS3 styling
- JavaScript engine (ES6+)
- Windows native GUI
- Dual target: MSVC and MinGW

## Build Instructions

### Prerequisites
- CMake 3.10 or higher
- A C++17 compatible compiler (MSVC, MinGW-w64, or GCC/Clang)

### Building with CMake
```bash
# Clone the repository
git clone <repository-url>
cd nitrogen-browser

# Create a build directory
mkdir build && cd build

# Configure the project
cmake ..

# Build the project
cmake --build . --config Release
```

## Project Structure
- `src/` - Source code files
- `include/` - Header files
- `tests/` - Unit tests
- `docs/` - Documentation
- `third_party/` - External libraries

## License
This project is licensed under the MIT License - see the LICENSE file for details.