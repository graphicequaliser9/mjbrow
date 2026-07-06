/**
 * @file css/StringPool.h
 * @brief Interned string pool for memory-efficient CSS parsing.
 * @details Each unique string is stored exactly once.  All selector, property,
 *          and value strings returned by the pool share their storage with
 *          every other reference to the same sequence of characters, eliminating
 *          duplicated heap allocations.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef CSS_STRINGPOOL_H
#define CSS_STRINGPOOL_H

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace css {

/**
 * @class StringPool
 * @brief Interns strings so that equal strings share one storage location.
 */
class StringPool {
public:
    StringPool() = default;
    ~StringPool() = default;

    StringPool(const StringPool&) = delete;
    StringPool& operator=(const StringPool&) = default;

    const char* intern(const std::string& str);
    const char* intern(const char* data, size_t len);

    size_t size() const { return strings_.size(); }
    size_t bytesUsed() const;

    void clear() { strings_.clear(); index_.clear(); }

private:
    std::vector<std::string> strings_;
    std::unordered_map<std::string, size_t> index_;
};

} // namespace css

#endif // CSS_STRINGPOOL_H
