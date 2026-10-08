#include "search.hpp"

#include <algorithm>
#include <atomic>
#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace museum::search {
namespace {

inline constexpr std::size_t kResultLimit = 5;

template <class Function>
void ForEachWord(std::string_view text, Function function) {
    auto begin = text.find_first_not_of(' ');
    while (begin != std::string_view::npos) {
        auto end = text.find(' ', begin);
        if (end == std::string_view::npos) end = text.size();
        function(text.substr(begin, end - begin));
        begin = text.find_first_not_of(' ', end);
    }
}

void CheckRead(const std::istream& input) {
    if (input.bad() || (input.fail() && !input.eof())) {
        throw std::runtime_error("failed to read input stream");
    }
}

}  // namespace

Index::Index(std::istream& documents) {
    std::unordered_map<std::string, std::vector<Hit>> dictionary;
    for (std::string line; std::getline(documents, line); ++document_count_) {
        ForEachWord(line, [&](std::string_view word) {
            auto& postings = dictionary[std::string(word)];
            if (postings.empty() || postings.back().document_id != document_count_) {
                postings.push_back({document_count_, 1});
            } else {
                ++postings.back().count;
            }
        });
    }
    CheckRead(documents);
    terms_.reserve(dictionary.size());
    // Node handles let the final dictionary own the strings without copying them.
    while (!dictionary.empty()) {
        auto node = dictionary.extract(dictionary.begin());
        terms_.push_back({std::move(node.key()), std::move(node.mapped())});
    }
    std::sort(terms_.begin(), terms_.end(), [](const Term& a, const Term& b) {
        return a.word < b.word;
    });
}

std::vector<Hit> Index::Find(std::string_view query, Workspace& workspace) const {
    auto& counts = workspace.counts_;
    auto& touched = workspace.touched_;
    // Clear only documents touched by the previous query, even after an exception.
    if (counts.size() != document_count_) {
        counts.assign(document_count_, 0);
    } else {
        for (const auto& hit : touched) counts[hit.document_id] = 0;
    }
    touched.clear();

    ForEachWord(query, [&](std::string_view word) {
        const auto term = std::lower_bound(terms_.begin(), terms_.end(), word,
            [](const Term& entry, std::string_view key) { return entry.word < key; });
        if (term == terms_.end() || term->word != word) return;
        for (const auto& hit : term->postings) {
            auto& count = counts[hit.document_id];
            // Record first, so allocation failure cannot leave an untracked count.
            if (count == 0) touched.push_back({hit.document_id, 0});
            count += hit.count;
        }
    });
    for (auto& hit : touched) hit.count = counts[hit.document_id];
    const auto end = touched.begin() + std::min(kResultLimit, touched.size());
    std::partial_sort(touched.begin(), end, touched.end(), MoreRelevant);
    return {touched.begin(), end};
}

SearchServer::SearchServer() {
    std::istringstream empty;
    current_ = std::make_shared<const Index>(empty);
}

SearchServer::SearchServer(std::istream& documents)
    : current_(std::make_shared<const Index>(documents)) {}

void SearchServer::UpdateDocumentBase(std::istream& documents) {
    auto replacement = std::make_shared<const Index>(documents);
    std::atomic_store(&current_, std::move(replacement));
}

std::vector<Hit> SearchServer::Find(std::string_view query, Workspace& workspace) const {
    const auto snapshot = std::atomic_load(&current_);
    return snapshot->Find(query, workspace);
}

void SearchServer::AddQueriesStream(std::istream& queries, std::ostream& results) const {
    Workspace workspace;
    for (std::string query; std::getline(queries, query);) {
        const auto hits = Find(query, workspace);
        results << query << ':';
        for (const auto& hit : hits) {
            results << " {docid: " << hit.document_id << ", hitcount: " << hit.count << '}';
        }
        results << '\n';
        if (!results) throw std::runtime_error("failed to write search results");
    }
    CheckRead(queries);
}

}  // namespace museum::search
