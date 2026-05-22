#include "core/Win32Window.h"

int main() {
    core::Win32Window window;
    window.create();
    return window.run();
}