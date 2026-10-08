#pragma once
#include "json.hpp"
#include <algorithm>
#include <map>
#include <optional>
#include <regex>
#include <unordered_map>
namespace museum::brown {
struct Spending {
    std::string category;
    int amount;
};
inline std::vector<Spending> LoadFromJson(std::istream &input) {
    std::vector<Spending> out;
    const auto document = json::Load(input);
    for (const auto &value : document.AsArray())
        out.push_back({value.At("category").AsString(), value.At("amount").AsInt()});
    return out;
}
namespace Xml {
struct Node {
    std::string name;
    std::map<std::string, std::string> attributes;
    std::vector<Node> children;
};
struct Document {
    Node root;
};
inline std::string Escape(std::string text) {
    std::string out;
    for (char c : text) {
        if (c == '&')
            out += "&amp;";
        else if (c == '"')
            out += "&quot;";
        else if (c == '<')
            out += "&lt;";
        else if (c == '>')
            out += "&gt;";
        else
            out += c;
    }
    return out;
}
inline std::string Unescape(std::string text) {
    const std::pair<std::string, std::string> entities[] = {
        {"&quot;", "\""}, {"&lt;", "<"}, {"&gt;", ">"}, {"&apos;", "'"}, {"&amp;", "&"}};
    for (const auto &[from, to] : entities) {
        std::size_t at = 0;
        while ((at = text.find(from, at)) != text.npos) {
            text.replace(at, from.size(), to);
            at += to.size();
        }
    }
    return text;
}
// Deliberately the statement's tiny spending XML dialect, not a general XML parser.
inline Document Load(std::istream &input) {
    std::string text{std::istreambuf_iterator<char>(input), {}};
    std::smatch match;
    if (!std::regex_match(text, match,
                          std::regex(R"(\s*<([A-Za-z_][A-Za-z_0-9]*)>\s*([\s\S]*)</\1>\s*)")))
        throw std::invalid_argument("invalid spending XML root");
    Document doc{{match[1], {}, {}}};
    std::string body = match[2];
    const std::regex element(R"(\s*<spend\s+([^>]*)>\s*</spend>\s*)"),
        attribute(R"re(([A-Za-z_]+)="([^"]*)")re");
    while (!body.empty()) {
        if (!std::regex_search(body, match, element, std::regex_constants::match_continuous))
            throw std::invalid_argument("invalid spend element");
        Node node{"spend", {}, {}};
        std::string attrs = match[1];
        for (auto it = std::sregex_iterator(attrs.begin(), attrs.end(), attribute);
             it != std::sregex_iterator(); ++it)
            node.attributes[(*it)[1]] = Unescape((*it)[2]);
        if (node.attributes.size() != 2 || !node.attributes.count("amount") ||
            !node.attributes.count("category"))
            throw std::invalid_argument("missing spending attributes");
        doc.root.children.push_back(std::move(node));
        body = match.suffix();
    }
    return doc;
}
inline void Print(const Document &doc, std::ostream &out) {
    out << '<' << doc.root.name << ">\n";
    for (const auto &node : doc.root.children)
        out << "  <spend amount=\"" << Escape(node.attributes.at("amount")) << "\" category=\""
            << Escape(node.attributes.at("category")) << "\"></spend>\n";
    out << "</" << doc.root.name << ">\n";
}
} // namespace Xml
inline std::vector<Spending> LoadFromXml(std::istream &input) {
    auto doc = Xml::Load(input);
    std::vector<Spending> out;
    for (const auto &node : doc.root.children)
        out.push_back({node.attributes.at("category"), std::stoi(node.attributes.at("amount"))});
    return out;
}
inline json::Node XmlToJson(const Xml::Document &doc) {
    json::Array out;
    for (const auto &node : doc.root.children)
        out.emplace_back(json::Object{{"category", node.attributes.at("category")},
                                      {"amount", std::stoi(node.attributes.at("amount"))}});
    return out;
}
inline Xml::Document JsonToXml(const json::Node &doc, std::string root) {
    Xml::Document out{{std::move(root), {}, {}}};
    for (const auto &item : doc.AsArray())
        out.root.children.push_back({"spend",
                                     {{"category", item.At("category").AsString()},
                                      {"amount", std::to_string(item.At("amount").AsInt())}},
                                     {}});
    return out;
}
namespace Ini {
using Section = std::unordered_map<std::string, std::string>;
class Document {
    std::unordered_map<std::string, Section> sections_;

  public:
    Section &AddSection(std::string name) { return sections_[std::move(name)]; }
    const Section &GetSection(const std::string &name) const { return sections_.at(name); }
    std::size_t SectionCount() const { return sections_.size(); }
};
inline Document Load(std::istream &input) {
    Document doc;
    Section *section = nullptr;
    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        if (line.front() == '[' && line.back() == ']')
            section = &doc.AddSection(line.substr(1, line.size() - 2));
        else {
            auto split = line.find('=');
            if (!section || split == line.npos)
                throw std::invalid_argument("invalid INI entry");
            (*section)[line.substr(0, split)] = line.substr(split + 1);
        }
    }
    return doc;
}
} // namespace Ini
class StatsAggregator {
  public:
    virtual ~StatsAggregator() = default;
    virtual void Process(int value) = 0;
    virtual std::optional<double> Get() const = 0;
};
namespace StatsAggregators {
class Sum : public StatsAggregator {
    long long value_ = 0;

  public:
    void Process(int v) override { value_ += v; }
    std::optional<double> Get() const override { return static_cast<double>(value_); }
};
class Min : public StatsAggregator {
    std::optional<int> value_;

  public:
    void Process(int v) override {
        if (!value_ || v < *value_)
            value_ = v;
    }
    std::optional<double> Get() const override {
        return value_ ? std::optional<double>(*value_) : std::nullopt;
    }
};
class Max : public StatsAggregator {
    std::optional<int> value_;

  public:
    void Process(int v) override {
        if (!value_ || v > *value_)
            value_ = v;
    }
    std::optional<double> Get() const override {
        return value_ ? std::optional<double>(*value_) : std::nullopt;
    }
};
class Average : public StatsAggregator {
    double sum_ = 0;
    std::size_t n_ = 0;

  public:
    void Process(int v) override {
        sum_ += v;
        ++n_;
    }
    std::optional<double> Get() const override {
        return n_ ? std::optional<double>(sum_ / n_) : std::nullopt;
    }
};
class Mode : public StatsAggregator {
    std::map<int, std::size_t> counts_;

  public:
    void Process(int v) override { ++counts_[v]; }
    std::optional<double> Get() const override {
        std::optional<double> result;
        std::size_t max = 0;
        for (auto [v, n] : counts_)
            if (n > max) {
                result = v;
                max = n;
            }
        return result;
    }
};
} // namespace StatsAggregators
} // namespace museum::brown
