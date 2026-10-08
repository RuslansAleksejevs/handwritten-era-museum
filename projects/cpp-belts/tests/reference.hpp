#pragma once

#include "../search/search.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace reference {

inline std::vector<std::string> Words(const std::string& text) {
    std::istringstream stream(text);
    std::vector<std::string> words;
    for (std::string word; stream >> word;) words.push_back(std::move(word));
    return words;
}

// Deliberately scan every document and fully sort. No inverted index or sparse state.
inline std::vector<museum::search::Hit> Search(
        const std::vector<std::vector<std::string>>& documents, const std::string& query) {
    const auto words = Words(query);
    std::vector<museum::search::Hit> result;
    for (std::size_t id = 0; id < documents.size(); ++id) {
        std::uint64_t count = 0;
        for (const auto& word : words) {
            count += std::count(documents[id].begin(), documents[id].end(), word);
        }
        if (count != 0) result.push_back({id, count});
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        if (a.count != b.count) return a.count > b.count;
        return a.document_id < b.document_id;
    });
    if (result.size() > 5) result.resize(5);
    return result;
}

inline bool Blocked(const std::vector<std::string>& banned, const std::string& query) {
    for (const auto& root : banned) {
        if (query == root) return true;
        if (query.size() > root.size() && query[query.size() - root.size() - 1] == '.' &&
            query.compare(query.size() - root.size(), root.size(), root) == 0) return true;
    }
    return false;
}

}  // namespace reference
