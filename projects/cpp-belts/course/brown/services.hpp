#pragma once
#include "../../domains/domains.hpp"
#include "../red/services.hpp"
#include <iostream>
#include <optional>
namespace museum::brown {
using museum::red::BookingManager;
using museum::red::Express;
using museum::red::ReadingManager;
struct Citizen {
    std::string name;
    int age, income;
    char gender;
};
class Demographics {
    std::vector<int> ages_;
    std::vector<long long> wealthy_{0};
    std::map<char, std::string> popular_;

  public:
    explicit Demographics(const std::vector<Citizen> &people) {
        std::vector<int> incomes;
        std::map<char, std::map<std::string, int>> names;
        for (const auto &person : people) {
            ages_.push_back(person.age);
            incomes.push_back(person.income);
            ++names[person.gender][person.name];
        }
        std::sort(ages_.begin(), ages_.end());
        std::sort(incomes.begin(), incomes.end(), std::greater<int>{});
        for (int income : incomes)
            wealthy_.push_back(wealthy_.back() + income);
        for (const auto &[gender, counts] : names) {
            int best = 0;
            for (const auto &[name, n] : counts)
                if (n > best) {
                    popular_[gender] = name;
                    best = n;
                }
        }
    }
    std::size_t Adults(int age) const {
        return ages_.end() - std::lower_bound(ages_.begin(), ages_.end(), age);
    }
    long long Wealthy(std::size_t n) const { return wealthy_.at(n); }
    std::optional<std::string> Popular(char gender) const {
        auto it = popular_.find(gender);
        return it == popular_.end() ? std::nullopt : std::optional<std::string>(it->second);
    }
};
namespace persons {
enum class Gender { FEMALE, MALE };
struct Person {
    int age;
    Gender gender;
    bool is_employed;
};
struct AgeStats {
    int total, females, males, employed_females, unemployed_females, employed_males,
        unemployed_males;
};
inline int Median(std::vector<int> ages) {
    if (ages.empty())
        return 0;
    auto middle = ages.begin() + ages.size() / 2;
    std::nth_element(ages.begin(), middle, ages.end());
    return *middle;
}
inline AgeStats ComputeStats(const std::vector<Person> &people) {
    std::array<std::vector<int>, 7> groups;
    for (const auto &p : people) {
        groups[0].push_back(p.age);
        const bool male = p.gender == Gender::MALE;
        groups[male ? 2 : 1].push_back(p.age);
        groups[male ? (p.is_employed ? 5 : 6) : (p.is_employed ? 3 : 4)].push_back(p.age);
    }
    return {Median(groups[0]), Median(groups[1]), Median(groups[2]), Median(groups[3]),
            Median(groups[4]), Median(groups[5]), Median(groups[6])};
}
inline void PrintStats(const AgeStats &stats, std::ostream &out = std::cout) {
    out << "Median age = " << stats.total << "\nMedian age for females = " << stats.females
        << "\nMedian age for males = " << stats.males
        << "\nMedian age for employed females = " << stats.employed_females
        << "\nMedian age for unemployed females = " << stats.unemployed_females
        << "\nMedian age for employed males = " << stats.employed_males
        << "\nMedian age for unemployed males = " << stats.unemployed_males << '\n';
}
inline void PrintStats(std::vector<Person> people) { PrintStats(ComputeStats(people)); }
} // namespace persons
enum class TaskStatus { NEW, IN_PROGRESS, TESTING, DONE };
using TasksInfo = std::map<TaskStatus, int>;
class TeamTasks {
    std::map<std::string, TasksInfo> people_;

  public:
    const TasksInfo &GetPersonTasksInfo(const std::string &name) const { return people_.at(name); }
    void AddNewTask(const std::string &name) { ++people_[name][TaskStatus::NEW]; }
    std::tuple<TasksInfo, TasksInfo> PerformPersonTasks(const std::string &name, int count) {
        TasksInfo updated, untouched;
        auto &current = people_[name];
        TasksInfo next;
        for (auto [status, n] : current) {
            if (status == TaskStatus::DONE) {
                next[status] += n;
                continue;
            }
            const int move = std::min(count, n);
            count -= move;
            if (move) {
                auto target = static_cast<TaskStatus>(static_cast<int>(status) + 1);
                updated[target] += move;
                next[target] += move;
            }
            if (n > move) {
                untouched[status] = n - move;
                next[status] += n - move;
            }
        }
        current = std::move(next);
        return {updated, untouched};
    }
};
namespace domains {
class Domain {
    std::string text_;
    std::vector<std::string> reverse_;

  public:
    explicit Domain(std::string text) : text_(std::move(text)) {
        std::size_t start = 0;
        while (start < text_.size()) {
            auto end = text_.find('.', start);
            reverse_.push_back(text_.substr(start, end - start));
            if (end == text_.npos)
                break;
            start = end + 1;
        }
        std::reverse(reverse_.begin(), reverse_.end());
    }
    const auto &GetReversedParts() const { return reverse_; }
    const std::string &Text() const { return text_; }
    bool IsSubdomain(const Domain &root) const {
        return reverse_.size() >= root.reverse_.size() &&
               std::equal(root.reverse_.begin(), root.reverse_.end(), reverse_.begin());
    }
};
class DomainChecker {
    museum::domains::Filter filter_;
    template <class It> static std::vector<std::string> Texts(It a, It b) {
        std::vector<std::string> out;
        for (; a != b; ++a)
            out.push_back(a->Text());
        return out;
    }

  public:
    template <class It> DomainChecker(It a, It b) : filter_(Texts(a, b)) {}
    bool IsForbidden(const Domain &domain) const { return filter_.IsBlocked(domain.Text()); }
    std::size_t RootCount() const { return filter_.RootCount(); }
};
} // namespace domains
enum class HttpCode { Ok = 200, NotFound = 404, Found = 302 };
class HttpResponse {
    HttpCode code_;
    std::vector<std::pair<std::string, std::string>> headers_;
    std::string content_;

  public:
    explicit HttpResponse(HttpCode code) : code_(code) {}
    HttpResponse &AddHeader(std::string name, std::string value) {
        headers_.emplace_back(std::move(name), std::move(value));
        return *this;
    }
    HttpResponse &SetContent(std::string text) {
        content_ = std::move(text);
        return *this;
    }
    HttpResponse &SetCode(HttpCode code) {
        code_ = code;
        return *this;
    }
    HttpCode Code() const { return code_; }
    const std::string &Content() const { return content_; }
    friend std::ostream &operator<<(std::ostream &out, const HttpResponse &response) {
        out << "HTTP/1.1 " << static_cast<int>(response.code_) << ' '
            << (response.code_ == HttpCode::Ok      ? "OK"
                : response.code_ == HttpCode::Found ? "Found"
                                                    : "Not found")
            << '\n';
        for (const auto &[name, value] : response.headers_)
            if (name != "Content-Length")
                out << name << ": " << value << '\n';
        if (!response.content_.empty())
            out << "Content-Length: " << response.content_.size() << '\n';
        return out << '\n' << response.content_;
    }
};
struct HttpRequest {
    std::string method, path, body;
    std::map<std::string, std::string> get_params;
};
// The archived statement omits the original request-body/captcha starter code.
// This explicit local protocol uses "user_id comment" and "user_id 42".
class CommentServer {
    std::vector<std::vector<std::string>> comments_;
    std::set<std::size_t> blocked_;
    std::optional<std::size_t> last_;
    int consecutive_ = 0;
    static HttpResponse Captcha() {
        return HttpResponse(HttpCode::Found).AddHeader("Location", "/captcha");
    }

  public:
    HttpResponse ServeRequest(const HttpRequest &request) {
        if (request.method == "POST" && request.path == "/add_user") {
            comments_.emplace_back();
            return HttpResponse(HttpCode::Ok).SetContent(std::to_string(comments_.size() - 1));
        }
        if (request.method == "GET" && request.path == "/captcha")
            return HttpResponse(HttpCode::Ok).SetContent("What is 6 * 7?");
        if (request.method == "GET" && request.path == "/user_comments") {
            auto found = request.get_params.find("user_id");
            if (found == request.get_params.end())
                return HttpResponse(HttpCode::NotFound);
            std::size_t user = 0;
            std::istringstream user_input(found->second);
            if (!(user_input >> user))
                return HttpResponse(HttpCode::NotFound);
            if (user >= comments_.size())
                return HttpResponse(HttpCode::NotFound);
            std::string text;
            for (const auto &comment : comments_[user])
                text += comment + '\n';
            return HttpResponse(HttpCode::Ok).SetContent(std::move(text));
        }
        if (request.method == "POST" &&
            (request.path == "/add_comment" || request.path == "/checkcaptcha")) {
            std::istringstream in(request.body);
            std::size_t user;
            if (!(in >> user) || user >= comments_.size())
                return HttpResponse(HttpCode::NotFound);
            if (request.path == "/checkcaptcha") {
                int answer = 0;
                if (in >> answer && answer == 42) {
                    blocked_.erase(user);
                    if (last_ == user)
                        consecutive_ = 0;
                    return HttpResponse(HttpCode::Ok);
                }
                return Captcha();
            }
            if (blocked_.count(user))
                return Captcha();
            if (last_ == user)
                ++consecutive_;
            else {
                last_ = user;
                consecutive_ = 1;
            }
            if (consecutive_ >= 3) {
                blocked_.insert(user);
                return Captcha();
            }
            std::string comment;
            std::getline(in >> std::ws, comment);
            comments_[user].push_back(std::move(comment));
            return HttpResponse(HttpCode::Ok);
        }
        return HttpResponse(HttpCode::NotFound);
    }
};
} // namespace museum::brown
