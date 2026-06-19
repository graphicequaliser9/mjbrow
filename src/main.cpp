/**
 * @file main.cpp
 * @brief Nitrogen browser entry point.  Accepts an optional CLI argument as the home URL.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "nitrogen.h"

#include <string>

#if defined(_WIN32) && defined(_MSC_VER)
#include <shellapi.h>
#include <windows.h>

namespace {

std::string WideToUtf8(const wchar_t* value) {
    if (!value) return {};

    const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};

    std::string result(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::string GetStartupUrl() {
    std::string homeUrl = "https://www.jacobsm.com/browtest3.htm";
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {
        homeUrl = WideToUtf8(argv[1]);
    }
    if (argv) {
        LocalFree(argv);
    }
    return homeUrl;
}

} // namespace
#endif

#if defined(_WIN32) && defined(_MSC_VER)
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
#else
int main(int argc, char* argv[]) {
#endif
    util::InitLogging();
    util::Log(util::LogLevel::Info, "Nitrogen Browser starting\n");

#if defined(_WIN32) && defined(_MSC_VER)
    std::string homeUrl = GetStartupUrl();
#else
    std::string homeUrl = "https://www.jacobsm.com/browtest3.htm";
    if (argc > 1) homeUrl = argv[1];
#endif

    browser::BrowserUI ui;
    ui.run(homeUrl);

    util::Log(util::LogLevel::Info, "Nitrogen Browser shutdown\n");
    util::ShutdownLogging();
    return 0;
}
