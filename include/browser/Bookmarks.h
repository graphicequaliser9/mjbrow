/**
 * @file Bookmarks.h
 * @brief Bookmarks bar data model and %APPDATA% JSON persistence.
 * @details Stores flat favourites and named folders.  Persisted as
 *          mjbrow_bookmarks.json in %APPDATA% on Windows (or ~/.config/mjbrow
 *          on other platforms).  Mutation calls automatically dirty the file so
 *          the next save() call flushes to disk.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef BROWSER_BOOKMARKS_H
#define BROWSER_BOOKMARKS_H

#include <string>
#include <vector>
#include <memory>

namespace browser {

/**
 * @struct BookmarkEntry
 * @brief One bookmarked URL (top-level or inside a folder).
 */
struct BookmarkEntry {
    std::string id;        ///< Stable identifier ("bm-001", etc.)
    std::string title;     ///< Display label
    std::string url;       ///< Full URL
    std::string parentId;  ///< "" for top-level; folder ID for folder children
    bool isFolder{false};  ///< True → this entry is itself a folder header
};

/**
 * @class Bookmarks
 * @brief Favourites manager with disk persistence on %APPDATA% / ~/.config.
 */
class Bookmarks {
public:
    Bookmarks();
    ~Bookmarks();

    // ── mutation ────────────────────────────────────────────────────────────────

    /**
     * @brief Adds a top-level bookmark.
     * @param title Display label.
     * @param url   Full URL string.
     * @return New entry ID.
     */
    std::string addBookmark(const std::string& title, const std::string& url);

    /**
     * @brief Adds a child bookmark inside a folder.
     * @param folderId The folder's entry ID.
     * @param title    Display label.
     * @param url      Full URL string.
     * @return New entry ID.
     */
    std::string addBookmarkInFolder(const std::string& folderId,
                                    const std::string& title,
                                    const std::string& url);

    /**
     * @brief Creates a new empty folder.
     * @param name Folder display name.
     * @return New folder entry ID.
     */
    std::string addFolder(const std::string& name);

    /**
     * @brief Removes an entry by its stable ID.
     * @param id Entry ID to remove.
     * @return True if the entry was found and removed.
     */
    bool removeEntry(const std::string& id);

    // ── queries ────────────────────────────────────────────────────────────────

    /**
     * @brief Returns all top-level children sorted by title.
     */
    std::vector<const BookmarkEntry*> topLevelEntries() const;

    /**
     * @brief Returns all children inside a given folder, sorted by title.
     * @param folderId Folder ID or empty string for top-level entries.
     */
    std::vector<const BookmarkEntry*> childrenOf(const std::string& folderId) const;

    /**
     * @brief Looks up an entry by its stable ID.
     * @return Pointer to entry or nullptr if not found.
     */
    const BookmarkEntry* findById(const std::string& id) const;

    // ── persistence ────────────────────────────────────────────────────────────

    /**
     * @brief Loads bookmarks from disk.  Does nothing if no file exists yet.
     */
    void load();

    /**
     * @brief Writes current entries to mjbrow_bookmarks.json on disk.
     */
    void save();

private:
    /**
     * @brief Generates a unique entry ID ("bm-NNNNNN").
     */
    std::string generateId();

    std::vector<std::unique_ptr<BookmarkEntry>> entries_;  ///< All entries
    bool dirty_{false};                          ///< True → save() should write
    std::string filePath_;                       ///< Resolved %APPDATA% path
};

} // namespace browser

#endif // BROWSER_BOOKMARKS_H
