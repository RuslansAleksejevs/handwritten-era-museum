#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <locale>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace museum::black::spreadsheet {
struct Position {
    static constexpr int MAX_ROWS = 16384, MAX_COLS = 16384;
    int row = -1, col = -1;
    bool IsValid() const { return row >= 0 && col >= 0 && row < MAX_ROWS && col < MAX_COLS; }
    friend bool operator<(Position a, Position b) {
        return std::tie(a.row, a.col) < std::tie(b.row, b.col);
    }
    friend bool operator==(Position a, Position b) { return a.row == b.row && a.col == b.col; }
    std::string ToString() const {
        if (!IsValid())
            return {};
        std::string name;
        for (int n = col + 1; n; n = (n - 1) / 26)
            name += char('A' + (n - 1) % 26);
        std::reverse(name.begin(), name.end());
        return name + std::to_string(row + 1);
    }
    static Position FromString(std::string_view s) {
        std::size_t i = 0;
        int c = 0, r = 0;
        while (i < s.size() && s[i] >= 'A' && s[i] <= 'Z') {
            if (c > MAX_COLS / 26 + 1)
                return {};
            c = c * 26 + s[i++] - 'A' + 1;
        }
        if (!c || i == s.size() || s[i] < '1' || s[i] > '9')
            return {};
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            if (r > MAX_ROWS / 10)
                return {};
            r = r * 10 + s[i++] - '0';
        }
        Position p{r - 1, c - 1};
        return i == s.size() && p.IsValid() ? p : Position{};
    }
};
struct Size {
    int rows = 0, cols = 0;
};
enum class FormulaError { Ref, Value, Div0 };
inline std::string_view ErrorText(FormulaError e) {
    if (e == FormulaError::Ref)
        return "#REF!";
    if (e == FormulaError::Value)
        return "#VALUE!";
    return "#DIV/0!";
}
using Value = std::variant<std::string, double, FormulaError>;
class FormulaException : public std::invalid_argument {
  public:
    using invalid_argument::invalid_argument;
};
class CircularDependencyException : public std::logic_error {
  public:
    CircularDependencyException() : logic_error("circular dependency") {}
};

namespace detail {
inline std::optional<double> Number(const std::string &s) {
    if (s.empty())
        return std::nullopt;
    std::istringstream in(s);
    in.imbue(std::locale::classic());
    double value;
    in >> std::noskipws >> value;
    if (!in || in.peek() != std::char_traits<char>::eof() || !std::isfinite(value))
        return std::nullopt;
    return value;
}
struct Token {
    char kind;
    std::string text;
    double number = 0;
    Position reference;
};
struct Formula {
    std::vector<Token> tokens;
    std::vector<std::size_t> program;
    std::vector<Position> references;
    void RefreshReferences() {
        std::set<Position> sorted;
        for (const auto &t : tokens)
            if (t.kind == 'r')
                sorted.insert(t.reference);
        references.assign(sorted.begin(), sorted.end());
    }
    std::string Text() const {
        std::string out = "=";
        for (const auto &t : tokens)
            out += t.kind == 'r' ? t.reference.ToString() : t.text;
        return out;
    }
};
inline Formula Parse(std::string_view expression) {
    Formula f;
    for (std::size_t i = 0; i < expression.size();) {
        const char c = expression[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            ++i;
            continue;
        }
        if (c >= 'A' && c <= 'Z') {
            const auto begin = i;
            while (i < expression.size() && expression[i] >= 'A' && expression[i] <= 'Z')
                ++i;
            const auto digits = i;
            while (i < expression.size() && expression[i] >= '0' && expression[i] <= '9')
                ++i;
            if (i == digits)
                throw FormulaException("reference needs a row");
            auto p = Position::FromString(expression.substr(begin, i - begin));
            f.tokens.push_back(
                {p.IsValid() ? 'r' : 'e', p.IsValid() ? p.ToString() : "#REF!", 0, p});
        } else if (c == '#' && expression.substr(i, 5) == "#REF!") {
            f.tokens.push_back({'e', "#REF!", 0, {}});
            i += 5;
        } else if ((c >= '0' && c <= '9') || c == '.') {
            const auto begin = i;
            while (i < expression.size() &&
                   ((expression[i] >= '0' && expression[i] <= '9') || expression[i] == '.'))
                ++i;
            if (i < expression.size() && (expression[i] == 'e' || expression[i] == 'E')) {
                ++i;
                if (i < expression.size() && (expression[i] == '+' || expression[i] == '-'))
                    ++i;
                while (i < expression.size() && expression[i] >= '0' && expression[i] <= '9')
                    ++i;
            }
            auto text = std::string(expression.substr(begin, i - begin));
            auto n = Number(text);
            if (!n)
                throw FormulaException("invalid number");
            f.tokens.push_back({'n', std::move(text), *n, {}});
        } else if (std::string_view("+-*/()").find(c) != std::string_view::npos) {
            f.tokens.push_back({c, std::string(1, c), 0, {}});
            ++i;
        } else
            throw FormulaException("invalid formula character");
    }
    std::vector<std::size_t> operators;
    const auto precedence = [](char op) {
        return op == '~' || op == '`' ? 3 : op == '*' || op == '/' ? 2 : 1;
    };
    bool want_operand = true;
    for (std::size_t i = 0; i < f.tokens.size(); ++i) {
        auto &token = f.tokens[i];
        char c = token.kind;
        if (c == 'n' || c == 'r' || c == 'e') {
            if (!want_operand)
                throw FormulaException("missing operator");
            f.program.push_back(i);
            want_operand = false;
        } else if (c == '(') {
            if (!want_operand)
                throw FormulaException("missing operator before parenthesis");
            operators.push_back(i);
        } else if (c == ')') {
            if (want_operand)
                throw FormulaException("missing operand");
            while (!operators.empty() && f.tokens[operators.back()].kind != '(') {
                f.program.push_back(operators.back());
                operators.pop_back();
            }
            if (operators.empty())
                throw FormulaException("unmatched parenthesis");
            operators.pop_back();
        } else {
            if (want_operand) {
                if (c != '+' && c != '-')
                    throw FormulaException("missing operand");
                token.kind = c == '-' ? '~' : '`';
                operators.push_back(i);
            } else {
                while (!operators.empty() && f.tokens[operators.back()].kind != '(' &&
                       precedence(f.tokens[operators.back()].kind) >= precedence(c)) {
                    f.program.push_back(operators.back());
                    operators.pop_back();
                }
                operators.push_back(i);
                want_operand = true;
            }
        }
    }
    if (want_operand)
        throw FormulaException("incomplete formula");
    while (!operators.empty()) {
        if (f.tokens[operators.back()].kind == '(')
            throw FormulaException("unmatched parenthesis");
        f.program.push_back(operators.back());
        operators.pop_back();
    }
    f.RefreshReferences();
    return f;
}
} // namespace detail

class Sheet {
  public:
    class Cell {
        friend class Sheet;
        const Sheet *sheet_ = nullptr;
        Position position_;
        std::string text_;
        std::optional<detail::Formula> formula_;
        mutable std::optional<Value> cache_;

      public:
        std::string GetText() const { return text_; }
        Value GetValue() const { return sheet_->GetValue(position_); }
        std::vector<Position> GetReferencedCells() const {
            return formula_ ? formula_->references : std::vector<Position>{};
        }
    };

  private:
    std::map<Position, Cell> cells_;
    std::map<Position, std::set<Position>> dependents_;
    static void Validate(Position p) {
        if (!p.IsValid())
            throw std::out_of_range("invalid cell position");
    }
    void Invalidate(Position position) {
        std::vector<Position> pending{position};
        std::set<Position> visited;
        while (!pending.empty()) {
            auto p = pending.back();
            pending.pop_back();
            if (!visited.insert(p).second)
                continue;
            if (auto it = cells_.find(p); it != cells_.end())
                it->second.cache_.reset();
            if (auto it = dependents_.find(p); it != dependents_.end())
                pending.insert(pending.end(), it->second.begin(), it->second.end());
        }
    }
    void CheckCycle(Position target, const std::vector<Position> &refs) const {
        auto pending = refs;
        std::set<Position> visited;
        while (!pending.empty()) {
            auto p = pending.back();
            pending.pop_back();
            if (p == target)
                throw CircularDependencyException();
            if (!visited.insert(p).second)
                continue;
            if (auto it = cells_.find(p); it != cells_.end() && it->second.formula_) {
                const auto &next = it->second.formula_->references;
                pending.insert(pending.end(), next.begin(), next.end());
            }
        }
    }
    Value CachedOrEmpty(Position p) const {
        auto it = cells_.find(p);
        return it == cells_.end() ? Value(std::string{}) : *it->second.cache_;
    }
    Value Evaluate(const Cell &cell) const {
        if (!cell.formula_)
            return cell.text_.substr(!cell.text_.empty() && cell.text_[0] == '\'');
        std::vector<double> stack;
        for (auto index : cell.formula_->program) {
            const auto &t = cell.formula_->tokens[index];
            if (t.kind == 'e')
                return FormulaError::Ref;
            if (t.kind == 'n') {
                stack.push_back(t.number);
                continue;
            }
            if (t.kind == 'r') {
                auto value = CachedOrEmpty(t.reference);
                if (auto error = std::get_if<FormulaError>(&value))
                    return *error;
                if (auto number = std::get_if<double>(&value))
                    stack.push_back(*number);
                else {
                    const auto &s = std::get<std::string>(value);
                    if (s.empty())
                        stack.push_back(0);
                    else if (auto n = detail::Number(s))
                        stack.push_back(*n);
                    else
                        return FormulaError::Value;
                }
                continue;
            }
            if (t.kind == '~' || t.kind == '`') {
                if (t.kind == '~')
                    stack.back() = -stack.back();
                continue;
            }
            auto right = stack.back();
            stack.pop_back();
            auto &left = stack.back();
            if (t.kind == '/' && right == 0)
                return FormulaError::Div0;
            if (t.kind == '+')
                left += right;
            else if (t.kind == '-')
                left -= right;
            else if (t.kind == '*')
                left *= right;
            else
                left /= right;
            if (!std::isfinite(left))
                return FormulaError::Div0;
        }
        return stack.back();
    }
    void Transform(bool rows, bool insert, int before, int count) {
        const int limit = rows ? Position::MAX_ROWS : Position::MAX_COLS;
        if (before < 0 || before >= limit || count < 0 || count > limit - before)
            throw std::out_of_range("structural edit range");
        if (!count)
            return;
        const auto move = [&](Position p) -> std::optional<Position> {
            int &coordinate = rows ? p.row : p.col;
            if (insert && coordinate >= before) {
                if (coordinate >= limit - count)
                    throw std::out_of_range("insertion exceeds sheet bounds");
                coordinate += count;
            } else if (!insert && coordinate >= before) {
                if (coordinate < before + count)
                    return std::nullopt;
                coordinate -= count;
            }
            return p;
        };
        std::map<Position, Cell> fresh;
        std::map<Position, std::set<Position>> reverse;
        for (const auto &[position, old] : cells_) {
            auto changed = move(position);
            if (!changed)
                continue;
            auto cell = old;
            cell.position_ = *changed;
            cell.cache_.reset();
            if (cell.formula_) {
                for (auto &token : cell.formula_->tokens)
                    if (token.kind == 'r') {
                        if (auto p = move(token.reference))
                            token.reference = *p;
                        else {
                            token.kind = 'e';
                            token.text = "#REF!";
                            token.reference = {};
                        }
                    }
                cell.formula_->RefreshReferences();
                cell.text_ = cell.formula_->Text();
                for (auto ref : cell.formula_->references)
                    reverse[ref].insert(*changed);
            }
            fresh.emplace(*changed, std::move(cell));
        }
        cells_.swap(fresh);
        dependents_.swap(reverse);
    }

  public:
    Sheet() = default;
    Sheet(const Sheet &) = delete;
    Sheet &operator=(const Sheet &) = delete;
    void SetCell(Position position, std::string text) {
        Validate(position);
        Cell fresh;
        fresh.sheet_ = this;
        fresh.position_ = position;
        if (text.size() > 1 && text[0] == '=') {
            fresh.formula_ = detail::Parse(std::string_view(text).substr(1));
            CheckCycle(position, fresh.formula_->references);
            text = fresh.formula_->Text();
        }
        fresh.text_ = std::move(text);
        Invalidate(position);
        if (auto it = cells_.find(position); it != cells_.end() && it->second.formula_)
            for (auto ref : it->second.formula_->references) {
                auto d = dependents_.find(ref);
                if (d != dependents_.end() && (d->second.erase(position), d->second.empty()))
                    dependents_.erase(d);
            }
        if (fresh.formula_)
            for (auto ref : fresh.formula_->references)
                dependents_[ref].insert(position);
        cells_.insert_or_assign(position, std::move(fresh));
    }
    const Cell *GetCell(Position position) const {
        Validate(position);
        auto it = cells_.find(position);
        return it == cells_.end() ? nullptr : &it->second;
    }
    Cell *GetCell(Position position) {
        return const_cast<Cell *>(std::as_const(*this).GetCell(position));
    }
    void ClearCell(Position position) {
        SetCell(position, {});
        cells_.erase(position);
    }
    Value GetValue(Position position) const {
        Validate(position);
        // Explicit DFS keeps long dependency chains off the native call stack.
        std::vector<std::pair<Position, bool>> pending{{position, false}};
        while (!pending.empty()) {
            auto [p, ready] = pending.back();
            pending.pop_back();
            auto it = cells_.find(p);
            if (it == cells_.end() || it->second.cache_)
                continue;
            const auto &cell = it->second;
            if (ready || !cell.formula_)
                cell.cache_ = Evaluate(cell);
            else {
                pending.emplace_back(p, true);
                for (auto ref : cell.formula_->references)
                    pending.emplace_back(ref, false);
            }
        }
        return CachedOrEmpty(position);
    }
    void InsertRows(int before, int count = 1) { Transform(true, true, before, count); }
    void InsertCols(int before, int count = 1) { Transform(false, true, before, count); }
    void DeleteRows(int first, int count = 1) { Transform(true, false, first, count); }
    void DeleteCols(int first, int count = 1) { Transform(false, false, first, count); }
    Size GetPrintableSize() const {
        Size size;
        for (const auto &[p, c] : cells_)
            if (!c.text_.empty()) {
                size.rows = std::max(size.rows, p.row + 1);
                size.cols = std::max(size.cols, p.col + 1);
            }
        return size;
    }
    void PrintTexts(std::ostream &out) const { Print(out, false); }
    void PrintValues(std::ostream &out) const { Print(out, true); }

  private:
    void Print(std::ostream &out, bool values) const {
        const auto size = GetPrintableSize();
        for (int row = 0; row < size.rows; ++row) {
            for (int col = 0; col < size.cols; ++col) {
                if (col)
                    out << '\t';
                const auto *cell = GetCell({row, col});
                if (!cell)
                    continue;
                if (!values)
                    out << cell->GetText();
                else {
                    auto value = cell->GetValue();
                    if (auto s = std::get_if<std::string>(&value))
                        out << *s;
                    else if (auto n = std::get_if<double>(&value))
                        out << *n;
                    else
                        out << ErrorText(std::get<FormulaError>(value));
                }
            }
            out << '\n';
        }
    }
};
} // namespace museum::black::spreadsheet
