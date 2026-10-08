#pragma once
#include "../brown/json.hpp"
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace museum::black {
// The merge policy is independent of protobuf-generated ownership/accessors.
// JSON values preserve structured attributes; the adapter uses course field names.
struct CompanySignal {
    std::uint64_t provider_id;
    museum::brown::json::Node company;
};
inline museum::brown::json::Node
MergeCompanies(const std::vector<CompanySignal> &signals,
               const std::map<std::uint64_t, std::uint32_t> &priorities) {
    namespace json = museum::brown::json;
    json::Object result;
    for (const auto &field :
         std::vector<std::string>{"address", "names", "phones", "urls", "working_time"}) {
        const bool repeated = field == "names" || field == "phones" || field == "urls";
        std::optional<std::uint32_t> best;
        json::Array values;
        std::set<std::string> seen;
        for (const auto &signal : signals) {
            if (!signal.company.Contains(field))
                continue;
            const auto &value = signal.company.At(field);
            if (value.IsNull() || (repeated && value.AsArray().empty()))
                continue;
            const auto priority = priorities.at(signal.provider_id);
            if (best && priority < *best)
                continue;
            if (!best || priority > *best) {
                values.clear();
                seen.clear();
                best = priority;
            }
            if (repeated) {
                for (const auto &v : value.AsArray())
                    if (seen.insert(json::Serialize(v)).second)
                        values.push_back(v);
            } else if (values.empty())
                values.push_back(value);
        }
        if (best)
            result[field] = repeated ? json::Node(values) : values.front();
    }
    return result;
}
} // namespace museum::black
