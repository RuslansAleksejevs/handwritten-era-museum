#include "foundations.hpp"
#include "phone_book.hpp"
#include "spreadsheet.hpp"
#include "transport.hpp"
#include "yellow_pages.hpp"
#include <iostream>
#include <limits>

namespace black = museum::black;
namespace json = museum::brown::json;
namespace sheet = black::spreadsheet;

int Integer(const json::Node &node) {
    const double x = node.AsNumber();
    if (!std::isfinite(x) || std::floor(x) != x || x < std::numeric_limits<int>::min() ||
        x > std::numeric_limits<int>::max())
        throw std::invalid_argument("integer required");
    return static_cast<int>(x);
}
json::Node PhoneBookJson(const black::PhoneBook &book, std::string_view prefix) {
    json::Array entries;
    for (const auto &contact : book.FindByNamePrefix(prefix)) {
        json::Object item{{"name", contact.name}};
        if (contact.birthday)
            item["birthday"] = json::Object{{"year", contact.birthday->year},
                                            {"month", contact.birthday->month},
                                            {"day", contact.birthday->day}};
        json::Array phones;
        for (const auto &phone : contact.phones)
            phones.emplace_back(phone);
        item["phones"] = std::move(phones);
        entries.emplace_back(std::move(item));
    }
    return entries;
}
json::Node Spreadsheet(const json::Node &script) {
    sheet::Sheet table;
    json::Array results;
    for (const auto &command : script.AsArray()) {
        const auto &operation = command.At("op").AsString();
        if (operation == "set")
            table.SetCell(sheet::Position::FromString(command.At("cell").AsString()),
                          command.At("text").AsString());
        else if (operation == "clear")
            table.ClearCell(sheet::Position::FromString(command.At("cell").AsString()));
        else if (operation == "get") {
            const auto position = sheet::Position::FromString(command.At("cell").AsString());
            auto value = table.GetValue(position);
            const auto *cell = table.GetCell(position);
            json::Object result{{"cell", position.ToString()},
                                {"text", cell ? cell->GetText() : ""}};
            if (auto s = std::get_if<std::string>(&value))
                result["value"] = *s;
            else if (auto n = std::get_if<double>(&value))
                result["value"] = *n;
            else
                result["error"] =
                    std::string(sheet::ErrorText(std::get<sheet::FormulaError>(value)));
            json::Array refs;
            if (cell)
                for (auto p : cell->GetReferencedCells())
                    refs.emplace_back(p.ToString());
            result["references"] = std::move(refs);
            results.emplace_back(std::move(result));
        } else {
            const auto first = Integer(command.At("first"));
            const auto count = command.Contains("count") ? Integer(command.At("count")) : 1;
            if (operation == "insert_rows")
                table.InsertRows(first, count);
            else if (operation == "insert_cols")
                table.InsertCols(first, count);
            else if (operation == "delete_rows")
                table.DeleteRows(first, count);
            else if (operation == "delete_cols")
                table.DeleteCols(first, count);
            else
                throw std::invalid_argument("unknown sheet operation");
        }
    }
    return results;
}
int main(int argc, char **argv) {
    try {
        if (argc < 2)
            throw std::invalid_argument(
                "usage: black sum|transport [G|H|I|J|K]|sheet|phonebook-encode|phonebook-decode "
                "[prefix]|merge");
        const std::string mode = argv[1];
        if (mode == "sum") {
            std::int64_t a, b;
            if (!(std::cin >> a >> b))
                throw std::invalid_argument("two int64 values required");
            try {
                std::cout << black::CheckedSum(a, b) << '\n';
            } catch (const std::overflow_error &) {
                std::cout << "Overflow!\n";
            }
        } else if (mode == "transport") {
            const std::string projection = argc > 2 ? argv[2] : "G";
            if (projection.size() != 1 ||
                std::string("GHIJK").find(projection[0]) == std::string::npos)
                throw std::invalid_argument("projection: G, H, I, J or K");
            json::Serialize(black::TransportAnswers(json::Load(std::cin), projection[0]),
                            std::cout);
            std::cout << '\n';
        } else if (mode == "sheet") {
            json::Serialize(Spreadsheet(json::Load(std::cin)), std::cout);
            std::cout << '\n';
        } else if (mode == "phonebook-encode") {
            std::vector<black::Contact> contacts;
            const auto input = json::Load(std::cin);
            for (const auto &item : input.AsArray()) {
                black::Contact c;
                c.name = item.At("name").AsString();
                if (item.Contains("birthday")) {
                    const auto &b = item.At("birthday");
                    c.birthday = black::Date{Integer(b.At("year")), Integer(b.At("month")),
                                             Integer(b.At("day"))};
                }
                for (const auto &p : item.At("phones").AsArray())
                    c.phones.push_back(p.AsString());
                contacts.push_back(std::move(c));
            }
            black::PhoneBook(std::move(contacts)).SaveTo(std::cout);
        } else if (mode == "phonebook-decode") {
            json::Serialize(
                PhoneBookJson(black::DeserializePhoneBook(std::cin), argc > 2 ? argv[2] : ""),
                std::cout);
            std::cout << '\n';
        } else if (mode == "merge") {
            const auto input = json::Load(std::cin);
            std::map<std::uint64_t, std::uint32_t> priorities;
            for (const auto &p : input.At("providers").AsArray()) {
                int id = Integer(p.At("id")), priority = Integer(p.At("priority"));
                if (id < 0 || priority < 0)
                    throw std::invalid_argument("negative provider id/priority");
                priorities[static_cast<std::uint64_t>(id)] = static_cast<std::uint32_t>(priority);
            }
            std::vector<black::CompanySignal> signals;
            for (const auto &s : input.At("signals").AsArray()) {
                const int id = Integer(s.At("provider_id"));
                if (id < 0)
                    throw std::invalid_argument("negative provider id");
                signals.push_back({static_cast<std::uint64_t>(id), s.At("company")});
            }
            json::Serialize(black::MergeCompanies(signals, priorities), std::cout);
            std::cout << '\n';
        } else
            throw std::invalid_argument("unknown mode");
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 2;
    }
}
