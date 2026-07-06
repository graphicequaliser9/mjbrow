/**
 * @file css/StringPool.cpp
 * @brief String pool implementation.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "css/StringPool.h"

namespace css {

const char* StringPool::intern(const std::string& str) {
    auto it = index_.find(str);
    if (it != index_.end()) return strings_[it->second].c_str();
    size_t idx = strings_.size();
    strings_.push_back(str);
    index_[strings_.back()] = idx;
    return strings_.back().c_str();
}

const char* StringPool::intern(const char* data, size_t len) {
    std::string tmp(data, len);
    return intern(tmp);
}

size_t StringPool::bytesUsed() const {
    size_t total = 0;
    for (const auto& s : strings_) total += s.size();
    return total;
}

} // namespace css
