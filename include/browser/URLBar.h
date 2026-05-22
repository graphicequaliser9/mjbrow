/**
 * @file URLBar.h
 * @brief URL / location bar with live suggestions from Google Suggest.
 * @details Renders the address bar text field.  On every keydown it debounces
 *          a GET to https://suggestqueries.google.com/complete/search?client=firefox&q=<query>,
 *          parses the JSONP response, and shows up to 5 suggestions in a
 *          popup dropdown.  Clicking Enter navigates the attached WebView.
 *          Focus toggles on Ctrl+L; Escape clears and dismisses the dropdown.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef BROWSER_URLBAR_H
#define BROWSER_URLBAR_H

#include <string>
#include <vector>
#include <functional>

namespace browser {

/**
 * @struct SuggestionItem
 * @brief One autocomplete suggestion line.
 */
struct SuggestionItem {
    std::string text;   ///< Full suggestion string
    std::string url;    ///< If non-empty, navigating this suggestion goes here
};

/**
 * @class URLBar
 * @brief Address bar with debounced live autocomplete.
 */
class URLBar {
public:
    using SelectionCallback = std::function<void(int index)>;
    using NavigateCallback  = std::function<void(const std::string& url)>;

    URLBar();
    ~URLBar();

    // ── lifecycle ───────────────────────────────────────────────────────────────

    /**
     * @brief Sets the URL currently displayed in the bar.
     * @param url The URL string (updated solely by the owning WebView; typing
     *            without Enter does NOT change this until the user commits).
     */
    void setCurrentUrl(const std::string& url);

    /**
     * @brief Returns the last URL that was committed (Enter / click).
     */
    std::string committedUrl() const;

    // ── user interaction ───────────────────────────────────────────────────────

    /**
     * @brief Called when the user types a character.
     *        Schedules a debounced fetch for suggestions.
     * @param ch The character typed.
     */
    void onKeyDown(char ch);

    /**
     * @brief Called for special keys (Up/Down/Enter/Escape/Backspace).
     * @param keyCode Virtual-key style code: VK_UP, VK_DOWN, VK_RETURN, VK_ESCAPE.
     */
    void onSpecialKey(int keyCode);

    /**
     * @brief Give the bar input focus.
     */
    void setFocus();

    /**
     * @brief Whether the suggestion dropdown is currently open.
     */
    bool isDropdownOpen() const { return !suggestions_.empty(); }

    // ── suggestion list ────────────────────────────────────────────────────────

    /**
     * @brief Returns the current suggestion list (may be empty).
     */
    std::vector<SuggestionItem> getSuggestions() const { return suggestions_; }

    /**
     * @brief Returns the currently highlighted suggestion index.
     */
    int highlightedSuggestion() const { return highlighted_; }

    // ── callbacks ──────────────────────────────────────────────────────────────

    /**
     * @brief Called when the user selects a suggestion by index.
     */
    void onSelection(SelectionCallback cb) { selectionCb_ = std::move(cb); }

    /**
     * @brief Called when the user presses Enter to navigate to a URL.
     */
    void onNavigate(NavigateCallback cb) { navigateCb_ = std::move(cb); }

    /**
     * @brief Returns the raw text as currently typed by the user (pre-Enter).
     */
    std::string currentInput() const { return inputText_; }

private:
    /**
     * @brief Fetches Google Suggest asynchronously; called by the timer.
     */
    void fetchSuggestions(const std::string& query);

    /**
     * @brief Shows the current suggestions in the dropdown (truncates to 5).
     * @param items Fetched suggestion items.
     */
    void showSuggestions(const std::vector<SuggestionItem>& items);

    /**
     * @brief High-pass filters a Google Suggest response; produces SuggestionItem list.
     * @param raw The raw response body from the API.
     * @return Parsed suggestions (0–5 items).
     */
    static std::vector<SuggestionItem> parseGoogleSuggest(const std::string& raw);

    std::string committedUrl_;           ///< Last-committed URL (displayed)
    std::string inputText_;              ///< Text being typed right now
    std::vector<SuggestionItem> suggestions_;  ///< Current dropdown items
    int highlighted_{-1};                ///< Keyboard-hovered suggestion index
    bool focused_{false};                ///< Input focus flag

    SelectionCallback selectionCb_;
    NavigateCallback  navigateCb_;

    // Debounce timer stub — the platform layer owns the real Win32 timer.
    int  debounceMs_{300};               ///< Suggestions debounce interval
    bool fetchPending_{false};
};

} // namespace browser

#endif // BROWSER_URLBAR_H
