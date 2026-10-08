#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace museum::search {

struct Hit {
    std::size_t document_id;
    std::uint64_t count;
};

inline bool operator==(const Hit& left, const Hit& right) noexcept {
    return left.document_id == right.document_id && left.count == right.count;
}

inline bool MoreRelevant(const Hit& left, const Hit& right) noexcept {
    return left.count > right.count ||
           (left.count == right.count && left.document_id < right.document_id);
}

// Reuse on one reader thread. It owns its storage and borrows nothing from an index.
class Workspace {
    friend class Index;
    std::vector<std::uint64_t> counts_;
    std::vector<Hit> touched_;
};

// Owns every dictionary word. Read-only after construction; safe to share.
class Index {
public:
    explicit Index(std::istream& documents);
    std::vector<Hit> Find(std::string_view query, Workspace& workspace) const;
    std::size_t DocumentCount() const noexcept { return document_count_; }

private:
    struct Term {
        std::string word;
        std::vector<Hit> postings;
    };
    std::vector<Term> terms_;
    std::size_t document_count_ = 0;
};

// Calls are synchronous: the caller owns task scheduling and stream lifetimes.
// Readers may run concurrently with a replacement built by another thread.
class SearchServer {
public:
    SearchServer();
    explicit SearchServer(std::istream& documents);
    SearchServer(const SearchServer&) = delete;
    SearchServer& operator=(const SearchServer&) = delete;
    SearchServer(SearchServer&&) = delete;
    SearchServer& operator=(SearchServer&&) = delete;

    void UpdateDocumentBase(std::istream& documents);
    void AddQueriesStream(std::istream& queries, std::ostream& results) const;
    std::vector<Hit> Find(std::string_view query, Workspace& workspace) const;

private:
    std::shared_ptr<const Index> current_;
};

}  // namespace museum::search
