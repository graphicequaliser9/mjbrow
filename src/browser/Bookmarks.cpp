/**
 * @file Bookmarks.cpp
 * @brief Bookmarks data model and %APPDATA% JSON persistence implementation.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "browser/Bookmarks.h"
#include "util/String.h"
#include "util/Logging.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <system_error>

#ifdef _MSC_VER
#pragma warning(disable : 4996)  // getenv is deprecated in MSVC, but we use it for portability
#endif

namespace browser {

// ── mini JSON helpers (no external deps) ─────────────────────────────────

namespace {

/** Escapes a string for safe embedding inside a JSON string literal. */
std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

/** Writes a barebones JSON object string field: "key": "escaped_value",  */
void writeJsonField(std::ofstream& f, const std::string& key, const std::string& val) {
    f << "  \"" << key << "\": \"" << jsonEscape(val) << "\",\n";
}

} // anonymous namespace

// ── Bookmarks implementation ─────────────────────────────────────────────

Bookmarks::Bookmarks() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    filePath_ = (appdata ? appdata : ".") + std::string("\\mjbrow_bookmarks.json");
#else
    const char* home = std::getenv("HOME");
    filePath_ = (home ? home : ".") + std::string("/.config/mjbrow/mjbrow_bookmarks.json");
#endif
}

Bookmarks::~Bookmarks() {
    save();
}

std::string Bookmarks::generateId() {
    static int counter = 1;
    return "bm-" + std::to_string(counter++);
}

std::string Bookmarks::addBookmark(const std::string& title, const std::string& url) {
    auto entry = std::make_unique<BookmarkEntry>();
    entry->id      = generateId();
    entry->title   = title;
    entry->url     = url;
    entry->parentId = "";
    entry->isFolder = false;
    entries_.push_back(std::move(entry));
    dirty_ = true;
    return entries_.back()->id;
}

std::string Bookmarks::addBookmarkInFolder(const std::string& folderId,
                                            const std::string& title,
                                            const std::string& url) {
    auto entry = std::make_unique<BookmarkEntry>();
    entry->id       = generateId();
    entry->title    = title;
    entry->url      = url;
    entry->parentId = folderId;
    entry->isFolder = false;
    entries_.push_back(std::move(entry));
    dirty_ = true;
    return entries_.back()->id;
}

std::string Bookmarks::addFolder(const std::string& name) {
    auto entry = std::make_unique<BookmarkEntry>();
    entry->id       = generateId();
    entry->title    = name;
    entry->url      = "";
    entry->parentId = "";
    entry->isFolder = true;
    entries_.push_back(std::move(entry));
    dirty_ = true;
    return entries_.back()->id;
}

bool Bookmarks::removeEntry(const std::string& id) {
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if ((*it)->id == id) {
            entries_.erase(it);
            dirty_ = true;
            return true;
        }
    }
    return false;
}

std::vector<const BookmarkEntry*> Bookmarks::topLevelEntries() const {
    std::vector<const BookmarkEntry*> result;
    for (const auto& e : entries_) {
        if (e->parentId.empty()) {
            result.push_back(e.get());
        }
    }
    std::sort(result.begin(), result.end(),
              [](const auto* a, const auto* b) { return a->title < b->title; });
    return result;
}

std::vector<const BookmarkEntry*> Bookmarks::childrenOf(const std::string& folderId) const {
    std::vector<const BookmarkEntry*> result;
    for (const auto& e : entries_) {
        if (!e->parentId.empty() && e->parentId == folderId) {
            result.push_back(e.get());
        }
    }
    std::sort(result.begin(), result.end(),
              [](const auto* a, const auto* b) { return a->title < b->title; });
    return result;
}

const BookmarkEntry* Bookmarks::findById(const std::string& id) const {
    for (const auto& e : entries_) {
        if (e->id == id) return e.get();
    }
    return nullptr;
}

void Bookmarks::load() {
    try {
        std::ifstream f(filePath_);
        if (!f.is_open()) {
            util::Log(util::LogLevel::Info, "Bookmarks: no saved file at " + filePath_ + "\n");
            return;
        }

        std::string line;
        std::string content;
        while (std::getline(f, line)) {
            content += line + "\n";
        }
        f.close();

        // Find each entry block by scanning for "{" ... "}," sequences
        size_t searchPos = 0;
        while (true) {
            auto openBrace = content.find('{', searchPos);
            if (openBrace == std::string::npos) break;
            auto closeBrace = content.find('}', openBrace);
            if (closeBrace == std::string::npos) break;

            std::string block = content.substr(openBrace, closeBrace - openBrace + 1);
            searchPos = closeBrace + 1;

            auto findInBlock = [&](const std::string& key) -> std::string {
                std::string marker = "\"" + key + "\"";
                auto pos = block.find(marker);
                if (pos == std::string::npos) return "";
                pos = block.find('"', block.find(':', pos) + 1);
                if (pos == std::string::npos) return "";
                ++pos;
                auto end = block.find('"', pos);
                if (end == std::string::npos) return "";
                return block.substr(pos, end - pos);
            };

            auto findBoolInBlock = [&](const std::string& key) -> bool {
                std::string marker = "\"" + key + "\"";
                auto pos = block.find(marker);
                if (pos == std::string::npos) return false;
                pos = block.find(':', pos);
                if (pos == std::string::npos) return false;
                ++pos;
                while (pos < block.size() && (block[pos] == ' ' || block[pos] == '\t')) ++pos;
                return block[pos] == 't' || block[pos] == '1';
            };

            std::string id = findInBlock("id");
            if (id.empty()) continue;

            auto entry = std::make_unique<BookmarkEntry>();
            entry->id       = id;
            entry->title    = findInBlock("title");
            entry->url      = findInBlock("url");
            entry->parentId = findInBlock("parentId");
            entry->isFolder = findBoolInBlock("isFolder");
            entries_.push_back(std::move(entry));
        }

        dirty_ = false;
        util::Log(util::LogLevel::Info,
                  "Bookmarks: loaded " + std::to_string(entries_.size()) + " entries\n");
    } catch (std::system_error& e) {
        util::Log(util::LogLevel::Error,
                  "Bookmarks: failed to load " + filePath_ + ": " + e.what() + "\n");
    }
}

void Bookmarks::save() {
    try {
        if (!dirty_) return;

        std::ofstream f(filePath_);
        if (!f.is_open()) {
            util::Log(util::LogLevel::Error, "Bookmarks: cannot open " + filePath_ + " for writing\n");
            return;
        }

        f << "{\n  \"entries\": [\n";
        for (size_t i = 0; i < entries_.size(); ++i) {
            const auto& e = entries_[i];
            f << "    {\n";
            writeJsonField(f, "id",        e->id);
            writeJsonField(f, "title",     e->title);
            writeJsonField(f, "url",       e->url);
            writeJsonField(f, "parentId",  e->parentId);
            f << "    \"isFolder\": " << (e->isFolder ? "true" : "false") << "\n";
            f << "    }" << (i + 1 < entries_.size() ? "," : "") << "\n";
        }
        f << "  ]\n}\n";
        f.close();
        dirty_ = false;
    } catch (std::system_error& e) {
        util::Log(util::LogLevel::Error,
                  "Bookmarks: failed to save " + filePath_ + ": " + e.what() + "\n");
    }
}

} // namespace browser
