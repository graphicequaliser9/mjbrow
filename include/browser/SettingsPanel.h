/**
 * @file SettingsPanel.h
 * @brief Browser settings overlay panel (opened with Ctrl+,).
 * @details Provides user-configurable options:
 *          - Hardware acceleration: GDI vs. Direct2D
 *          - Default font family and font size
 *          - Custom user agent string
 *          Changes are applied immediately and persisted to disk.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef BROWSER_SETTINGSPANEL_H
#define BROWSER_SETTINGSPANEL_H

#include <string>
#include <functional>

namespace browser {

/**
 * @struct BrowserSettings
 * @brief All user-configurable browser settings (serialisable to / from JSON).
 */
struct BrowserSettings {
    // ── rendering ──────────────────────────────────────────────────────────────
    enum class AccelMode {
        GDI,        ///< GDI+ rendering (compatible with all Windows versions)
        Direct2D    ///< Direct2D / DirectWrite rendering (faster, requires Win7+)
    } accelMode{AccelMode::GDI};

    // ── typography ────────────────────────────────────────────────────────────
    std::string fontFamily{"Arial"};
    int         fontSize{16};  ///< Default font size in pixels

    // ── networking ────────────────────────────────────────────────────────────
    std::string userAgent{"Nitrogen/0.1"};
};

/**
 * @class SettingsPanel
 * @brief UI overlay for editing BrowserSettings.
 * @details The panel is modal: it takes focus when opened and returns focus
 *          to the URL bar when closed.  Settings are saved to
 *          %APPDATA%/mjbrow_settings.json on every change.
 */
class SettingsPanel {
public:
    using ChangeCallback = std::function<void(const BrowserSettings&)>;

    SettingsPanel(bool loadOnConstruct = true);
    ~SettingsPanel();

    /**
     * @brief Opens the settings panel and takes focus.
     */
    void open();

    /**
     * @brief Closes the settings panel and releases focus.
     */
    void close();

    /**
     * @brief Returns the current settings (reads from disk on first call).
     */
    const BrowserSettings& settings() const { return currentSettings_; }

    /**
     * @brief Loads settings from disk, leaving defaults unchanged if unavailable.
     */
    void load();

    /**
     * @brief Whether the panel is currently open.
     */
    bool isOpen() const { return open_; }

    /**
     * @brief Registers a callback fired on every settings change.
     */
    void onChange(ChangeCallback cb) { onChangeCb_ = std::move(cb); }

private:
    void applyFontFamily(const std::string& family);
    void applyFontSize(int size);
    void applyAccelMode(BrowserSettings::AccelMode mode);
    void applyUserAgent(const std::string& ua);

    void save();

    BrowserSettings currentSettings_;
    bool            open_{false};
    ChangeCallback  onChangeCb_;
};

} // namespace browser

#endif // BROWSER_SETTINGSPANEL_H
