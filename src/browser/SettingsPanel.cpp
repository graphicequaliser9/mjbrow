/**
 * @file SettingsPanel.cpp
 * @brief Settings panel implementation: GDI/Direct2D toggle, font, user agent,
 *        JSON persistence to %APPDATA%/mjbrow_settings.json.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "browser/SettingsPanel.h"

#include "util/Logging.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace browser {

SettingsPanel::SettingsPanel() { load(); }
SettingsPanel::~SettingsPanel() = default;

void SettingsPanel::open()  { open_ = true; }
void SettingsPanel::close() { open_ = false; }

void SettingsPanel::applyFontFamily(const std::string& family) {
    currentSettings_.fontFamily = family;
    util::Log(util::LogLevel::Info, "Settings: fontFamily=" + family + "\n");
    if (onChangeCb_) onChangeCb_(currentSettings_);
    save();
}

void SettingsPanel::applyFontSize(int size) {
    currentSettings_.fontSize = std::max(8, std::min(size, 72));
    util::Log(util::LogLevel::Info,
              "Settings: fontSize=" + std::to_string(currentSettings_.fontSize) + "\n");
    if (onChangeCb_) onChangeCb_(currentSettings_);
    save();
}

void SettingsPanel::applyAccelMode(BrowserSettings::AccelMode mode) {
    currentSettings_.accelMode = mode;
    std::string label = (mode == BrowserSettings::AccelMode::Direct2D) ? "Direct2D" : "GDI";
    util::Log(util::LogLevel::Info, "Settings: accelMode=" + label + "\n");
    if (onChangeCb_) onChangeCb_(currentSettings_);
    save();
}

void SettingsPanel::applyUserAgent(const std::string& ua) {
    currentSettings_.userAgent = ua;
    util::Log(util::LogLevel::Info, "Settings: userAgent=" + ua + "\n");
    if (onChangeCb_) onChangeCb_(currentSettings_);
    save();
}

// ── persistence ─────────────────────────────────────────────────────────

void SettingsPanel::load() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    std::string path = (appdata ? appdata : ".") + std::string("\\mjbrow_settings.json");
#else
    const char* home = std::getenv("HOME");
    std::string path = (home ? home : ".") + std::string("/.config/mjbrow/mjbrow_settings.json");
#endif

    std::ifstream f(path);
    if (!f.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    f.close();

    auto grabStr = [&](const std::string& key) -> std::string {
        std::string marker = "\"" + key + "\"";
        size_t pos = content.find(marker);
        if (pos == std::string::npos) return "";
        pos  = content.find(':', pos);
        if (pos == std::string::npos) return "";
        ++pos;
        while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t')) ++pos;
        if (pos < content.size() && content[pos] == '"') {
            ++pos;
            auto end = content.find('"', pos);
            return end == std::string::npos ? "" : content.substr(pos, end - pos);
        }
        return "";
    };

    std::string af = grabStr("accelMode");
    if (!af.empty()) currentSettings_.accelMode = (af == "Direct2D")
                                                   ? BrowserSettings::AccelMode::Direct2D
                                                   : BrowserSettings::AccelMode::GDI;

    std::string ff = grabStr("fontFamily");
    if (!ff.empty()) currentSettings_.fontFamily = ff;

    std::string fsStr = grabStr("fontSize");
    if (!fsStr.empty()) {
        try { currentSettings_.fontSize = std::stoi(fsStr); } catch (...) {}
    }

    std::string ua = grabStr("userAgent");
    if (!ua.empty()) currentSettings_.userAgent = ua;
}

void SettingsPanel::save() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    std::string path    = (appdata ? appdata : ".")
                        + std::string("\\mjbrow_settings.json");
#else
    const char* home    = std::getenv("HOME");
    std::string path    = (home ? home : ".")
                        + std::string("/.config/mjbrow/mjbrow_settings.json");
#endif

    std::ofstream f(path);
    if (!f.is_open()) return;

    std::string accelLabel = (currentSettings_.accelMode
                              == BrowserSettings::AccelMode::Direct2D) ? "Direct2D" : "GDI";

    f << "{\n"
      << "  \"accelMode\":   \"" << accelLabel      << "\",\n"
      << "  \"fontFamily\":  \"" << currentSettings_.fontFamily << "\",\n"
      << "  \"fontSize\":    "   << currentSettings_.fontSize   << ",\n"
      << "  \"userAgent\":   \"" << currentSettings_.userAgent  << "\"\n"
      << "}\n";
}

} // namespace browser
