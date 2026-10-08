#include "mython.hpp"
#include <algorithm>
#include <charconv>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <variant>

namespace museum::mython {
namespace {
constexpr std::size_t kSourceLimit = 1024 * 1024;
constexpr std::size_t kTokenLimit = 20000;
constexpr std::size_t kNodeLimit = 8192;
constexpr std::size_t kDepthLimit = 256; // parser nesting
// The CLI opts into more runtime frames; embedding keeps the old default.
// A method call consumes several frames, and stack use depends on the build.
constexpr std::size_t kMaxRuntimeDepthLimit = 2048;
constexpr std::size_t kStepLimit = 1000000;

[[noreturn]] void Fail(std::size_t line, const std::string &reason) {
    throw Error("line " + std::to_string(line) + ": " + reason);
}
bool Digit(char c) { return c >= '0' && c <= '9'; }
bool IdStart(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool IdPart(char c) { return IdStart(c) || Digit(c); }
const std::map<std::string, Kind> kKeywords = {
    {"class", Kind::Class}, {"def", Kind::Def},   {"return", Kind::Return},
    {"if", Kind::If},       {"else", Kind::Else}, {"print", Kind::Print},
    {"or", Kind::Or},       {"and", Kind::And},   {"not", Kind::Not},
    {"None", Kind::None},   {"True", Kind::True}, {"False", Kind::False}};
} // namespace

Lexer::Lexer(std::istream &source) {
    std::size_t line_no = 0, indentation = 0;
    std::string source_text;
    char byte;
    while (source.get(byte)) {
        if (source_text.size() == kSourceLimit)
            Fail(1, "source size limit exceeded");
        source_text += byte;
    }
    if (source.bad())
        Fail(1, "input read failed");
    std::istringstream input(source_text);
    auto add = [&](Kind kind, const std::string &text, std::int64_t number, std::size_t column) {
        if (tokens_.size() >= kTokenLimit)
            Fail(line_no, "token limit exceeded");
        tokens_.push_back({kind, text, number, line_no, column});
    };
    std::string line;
    while (std::getline(input, line)) {
        ++line_no;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::size_t cursor = 0;
        while (cursor < line.size() && line[cursor] == ' ')
            ++cursor;
        if (cursor == line.size() || line[cursor] == '#')
            continue;
        if (line[cursor] == '\t')
            Fail(line_no, "tabs are not indentation");
        if (cursor % 2)
            Fail(line_no, "indentation must use pairs of spaces");
        const auto current_indent = cursor / 2;
        while (indentation < current_indent) {
            add(Kind::Indent, "", 0, 1);
            ++indentation;
        }
        while (indentation > current_indent) {
            add(Kind::Dedent, "", 0, 1);
            --indentation;
        }
        while (cursor < line.size()) {
            char c = line[cursor];
            if (c == ' ') {
                ++cursor;
                continue;
            }
            if (c == '#')
                break;
            const std::size_t column = cursor + 1;
            if (Digit(c)) {
                const auto begin = cursor;
                while (cursor < line.size() && Digit(line[cursor]))
                    ++cursor;
                if (cursor < line.size() && IdStart(line[cursor]))
                    Fail(line_no, "invalid integer suffix");
                std::int64_t value = 0;
                const auto parsed =
                    std::from_chars(line.data() + begin, line.data() + cursor, value);
                if (parsed.ec != std::errc{})
                    Fail(line_no, "integer literal overflow");
                add(Kind::Number, line.substr(begin, cursor - begin), value, column);
            } else if (IdStart(c)) {
                const auto begin = cursor++;
                while (cursor < line.size() && IdPart(line[cursor]))
                    ++cursor;
                auto name = line.substr(begin, cursor - begin);
                auto keyword = kKeywords.find(name);
                add(keyword == kKeywords.end() ? Kind::Identifier : keyword->second, name, 0,
                    column);
            } else if (c == '\'' || c == '"') {
                const char quote = c;
                ++cursor;
                std::string value;
                bool closed = false;
                while (cursor < line.size()) {
                    c = line[cursor++];
                    if (c == quote) {
                        closed = true;
                        break;
                    }
                    if (c == '\\') {
                        if (cursor == line.size())
                            Fail(line_no, "unfinished string escape");
                        c = line[cursor++];
                        switch (c) {
                        case 'n':
                            value += '\n';
                            break;
                        case 't':
                            value += '\t';
                            break;
                        case 'r':
                            value += '\r';
                            break;
                        case '\\':
                        case '\'':
                        case '"':
                            value += c;
                            break;
                        default:
                            Fail(line_no, "unknown string escape");
                        }
                    } else {
                        value += c;
                    }
                }
                if (!closed)
                    Fail(line_no, "unterminated string");
                add(Kind::String, value, 0, column);
            } else {
                std::string symbol(1, c);
                ++cursor;
                if (cursor < line.size() && line[cursor] == '=' &&
                    (c == '=' || c == '!' || c == '<' || c == '>')) {
                    symbol += '=';
                    ++cursor;
                } else if (std::string(".,:()=+-*/<>").find(c) == std::string::npos) {
                    Fail(line_no, "unexpected character");
                }
                add(Kind::Symbol, symbol, 0, column);
            }
        }
        add(Kind::Newline, "", 0, line.size() + 1);
    }
    if (source.bad())
        Fail(line_no, "input read failed");
    ++line_no;
    while (indentation) {
        add(Kind::Dedent, "", 0, 1);
        --indentation;
    }
    add(Kind::Eof, "", 0, 1);
}
const Token &Lexer::CurrentToken() const { return tokens_[position_]; }
const Token &Lexer::NextToken() {
    if (position_ + 1 < tokens_.size())
        ++position_;
    return CurrentToken();
}
const Token &Lexer::Expect(Kind kind) const {
    if (CurrentToken().kind != kind)
        Fail(CurrentToken().line, "unexpected token '" + CurrentToken().text + "'");
    return CurrentToken();
}
const Token &Lexer::Expect(Kind kind, const std::string &text) const {
    const auto &token = Expect(kind);
    if (token.text != text)
        Fail(token.line, "expected '" + text + "'");
    return token;
}
const Token &Lexer::ExpectNext(Kind kind) {
    NextToken();
    return Expect(kind);
}
const Token &Lexer::ExpectNext(Kind kind, const std::string &text) {
    NextToken();
    return Expect(kind, text);
}

namespace {
struct Class;
struct Instance;
using Value = std::variant<std::monostate, std::int64_t, bool, std::string, Instance *, Class *>;
using Environment = std::unordered_map<std::string, Value>;
struct Expression;
struct Statement;
using Expr = std::shared_ptr<Expression>;
using Stmt = std::shared_ptr<Statement>;
struct Method {
    std::string name;
    std::vector<std::string> parameters;
    Stmt body;
};
struct Expression {
    enum class Type { Literal, Name, Attribute, Unary, Binary, Call } type;
    std::size_t line;
    std::string text;
    Value literal;
    Expr left, right;
    std::vector<Expr> arguments;
};
struct Statement {
    enum class Type { Block, Print, Assignment, Expression, Return, If, Class } type;
    std::size_t line;
    std::string name, parent;
    Expr expression, target;
    std::vector<Expr> arguments;
    std::vector<Stmt> statements;
    Stmt body, otherwise;
    std::vector<Method> methods;
};

// Guards both actual recursion and resource usage, with exception-safe unwinding.
struct DepthGuard {
    std::size_t &depth;
    DepthGuard(std::size_t &count, std::size_t line, std::size_t limit = kDepthLimit)
        : depth(count) {
        if (depth >= limit)
            Fail(line, "nesting/call depth limit exceeded");
        ++depth;
    }
    ~DepthGuard() { --depth; }
};

class Parser {
  public:
    explicit Parser(Lexer &lexer) : lexer_(lexer) {}
    Stmt Program() {
        auto block = Node(Statement::Type::Block);
        while (!Is(Kind::Eof))
            block->statements.push_back(ParseStatement());
        return block;
    }

  private:
    Lexer &lexer_;
    std::size_t depth_ = 0, nodes_ = 0, method_depth_ = 0;
    std::unordered_map<std::string, bool> classes_;
    bool Is(Kind kind) const { return lexer_.CurrentToken().kind == kind; }
    bool Symbol(const std::string &value) const {
        return Is(Kind::Symbol) && lexer_.CurrentToken().text == value;
    }
    bool Take(Kind kind) {
        if (!Is(kind))
            return false;
        lexer_.NextToken();
        return true;
    }
    bool TakeSymbol(const std::string &value) {
        if (!Symbol(value))
            return false;
        lexer_.NextToken();
        return true;
    }
    void Require(Kind kind) {
        lexer_.Expect(kind);
        lexer_.NextToken();
    }
    void RequireSymbol(const std::string &symbol) {
        lexer_.Expect(Kind::Symbol, symbol);
        lexer_.NextToken();
    }
    std::string Identifier() {
        std::string name = lexer_.Expect(Kind::Identifier).text;
        lexer_.NextToken();
        return name;
    }
    void Count() {
        if (++nodes_ > kNodeLimit)
            Fail(lexer_.CurrentToken().line, "syntax tree size limit exceeded");
    }
    Stmt Node(Statement::Type type) {
        Count();
        auto node = std::make_shared<Statement>();
        node->type = type;
        node->line = lexer_.CurrentToken().line;
        return node;
    }
    Expr Node(Expression::Type type, const Token &token) {
        Count();
        auto node = std::make_shared<Expression>();
        node->type = type;
        node->line = token.line;
        node->text = token.text;
        return node;
    }
    Stmt Suite() {
        Require(Kind::Newline);
        Require(Kind::Indent);
        auto block = Node(Statement::Type::Block);
        while (!Is(Kind::Dedent)) {
            if (Is(Kind::Eof))
                Fail(lexer_.CurrentToken().line, "unterminated suite");
            block->statements.push_back(ParseStatement());
        }
        Require(Kind::Dedent);
        if (block->statements.empty())
            Fail(block->line, "empty suite");
        return block;
    }
    Stmt ClassDefinition() {
        auto node = Node(Statement::Type::Class);
        Require(Kind::Class);
        if (method_depth_)
            Fail(node->line, "class definitions inside methods are unsupported");
        node->name = Identifier();
        if (classes_.count(node->name))
            Fail(node->line, "class already declared: " + node->name);
        if (TakeSymbol("(")) {
            node->parent = Identifier();
            RequireSymbol(")");
            if (!classes_.count(node->parent))
                Fail(node->line, "base class must be declared first");
        }
        classes_[node->name] = true;
        RequireSymbol(":");
        Require(Kind::Newline);
        Require(Kind::Indent);
        std::unordered_map<std::string, bool> methods;
        while (Take(Kind::Def)) {
            Method method;
            method.name = Identifier();
            if (methods.count(method.name))
                Fail(node->line, "duplicate method: " + method.name);
            methods[method.name] = true;
            RequireSymbol("(");
            std::unordered_map<std::string, bool> parameters;
            if (!Symbol(")")) {
                do {
                    auto name = Identifier();
                    if (name == "self" || parameters.count(name))
                        Fail(node->line, "self is implicit and parameter names must be unique");
                    parameters[name] = true;
                    method.parameters.push_back(std::move(name));
                } while (TakeSymbol(","));
            }
            RequireSymbol(")");
            RequireSymbol(":");
            ++method_depth_;
            method.body = Suite();
            --method_depth_;
            node->methods.push_back(std::move(method));
        }
        if (node->methods.empty())
            Fail(node->line, "class needs at least one method");
        Require(Kind::Dedent);
        return node;
    }
    Stmt ParseStatement() {
        DepthGuard guard(depth_, lexer_.CurrentToken().line);
        if (Is(Kind::Class))
            return ClassDefinition();
        if (Is(Kind::If)) {
            auto node = Node(Statement::Type::If);
            Require(Kind::If);
            node->expression = Test();
            RequireSymbol(":");
            node->body = Suite();
            if (Take(Kind::Else)) {
                RequireSymbol(":");
                node->otherwise = Suite();
            }
            return node;
        }
        if (Is(Kind::Return)) {
            auto node = Node(Statement::Type::Return);
            Require(Kind::Return);
            if (!method_depth_)
                Fail(node->line, "return outside a method");
            node->expression = Test();
            Require(Kind::Newline);
            return node;
        }
        if (Is(Kind::Print)) {
            auto node = Node(Statement::Type::Print);
            Require(Kind::Print);
            if (!Is(Kind::Newline))
                node->arguments = Arguments();
            Require(Kind::Newline);
            return node;
        }
        auto node = Node(Statement::Type::Expression);
        node->expression = Test();
        if (TakeSymbol("=")) {
            auto target = node->expression;
            if (target->type != Expression::Type::Name &&
                target->type != Expression::Type::Attribute)
                Fail(node->line, "assignment target must be a variable or field");
            if (target->type == Expression::Type::Name && target->text == "self")
                Fail(node->line, "cannot rebind implicit self");
            node->type = Statement::Type::Assignment;
            node->target = target;
            node->expression = Test();
        } else if (node->expression->type != Expression::Type::Call) {
            Fail(node->line, "expected assignment or method call");
        }
        Require(Kind::Newline);
        return node;
    }
    std::vector<Expr> Arguments() {
        std::vector<Expr> result;
        do {
            result.push_back(Test());
        } while (TakeSymbol(","));
        return result;
    }
    Expr Binary(Expr left, const Token &token, Expr right) {
        auto result = Node(Expression::Type::Binary, token);
        result->left = std::move(left);
        result->right = std::move(right);
        return result;
    }
    Expr Test() {
        DepthGuard guard(depth_, lexer_.CurrentToken().line);
        auto result = And();
        while (Is(Kind::Or)) {
            auto token = lexer_.CurrentToken();
            lexer_.NextToken();
            result = Binary(result, token, And());
        }
        return result;
    }
    Expr And() {
        auto result = Not();
        while (Is(Kind::And)) {
            auto token = lexer_.CurrentToken();
            lexer_.NextToken();
            result = Binary(result, token, Not());
        }
        return result;
    }
    Expr Not() {
        DepthGuard guard(depth_, lexer_.CurrentToken().line);
        if (!Is(Kind::Not))
            return Comparison();
        auto node = Node(Expression::Type::Unary, lexer_.CurrentToken());
        lexer_.NextToken();
        node->left = Not();
        return node;
    }
    Expr Comparison() {
        auto result = Sum();
        if (Symbol("==") || Symbol("!=") || Symbol("<") || Symbol(">") || Symbol("<=") ||
            Symbol(">=")) {
            auto token = lexer_.CurrentToken();
            lexer_.NextToken();
            result = Binary(result, token, Sum());
        }
        return result;
    }
    Expr Sum() {
        auto result = Product();
        while (Symbol("+") || Symbol("-")) {
            auto token = lexer_.CurrentToken();
            lexer_.NextToken();
            result = Binary(result, token, Product());
        }
        return result;
    }
    Expr Product() {
        auto result = Unary();
        while (Symbol("*") || Symbol("/")) {
            auto token = lexer_.CurrentToken();
            lexer_.NextToken();
            result = Binary(result, token, Unary());
        }
        return result;
    }
    Expr Unary() {
        DepthGuard guard(depth_, lexer_.CurrentToken().line);
        if (Symbol("-") || Symbol("+")) {
            auto node = Node(Expression::Type::Unary, lexer_.CurrentToken());
            lexer_.NextToken();
            node->left = Unary();
            return node;
        }
        auto result = Atom();
        while (Symbol(".") || Symbol("(")) {
            if (TakeSymbol(".")) {
                auto node = Node(Expression::Type::Attribute, lexer_.Expect(Kind::Identifier));
                lexer_.NextToken();
                node->left = result;
                result = node;
            } else {
                auto node = Node(Expression::Type::Call, lexer_.CurrentToken());
                RequireSymbol("(");
                node->left = result;
                if (!Symbol(")"))
                    node->arguments = Arguments();
                RequireSymbol(")");
                result = node;
            }
        }
        return result;
    }
    Expr Atom() {
        const auto token = lexer_.CurrentToken();
        if (TakeSymbol("(")) {
            auto result = Test();
            RequireSymbol(")");
            return result;
        }
        if (Is(Kind::Identifier)) {
            auto result = Node(Expression::Type::Name, token);
            lexer_.NextToken();
            return result;
        }
        auto result = Node(Expression::Type::Literal, token);
        switch (token.kind) {
        case Kind::Number:
            result->literal = token.number;
            break;
        case Kind::String:
            result->literal = token.text;
            break;
        case Kind::None:
            result->literal = std::monostate{};
            break;
        case Kind::True:
            result->literal = true;
            break;
        case Kind::False:
            result->literal = false;
            break;
        default:
            Fail(token.line, "expected expression");
        }
        lexer_.NextToken();
        return result;
    }
};

struct Class {
    std::string name;
    Class *parent = nullptr;
    std::unordered_map<std::string, Method> methods;
    const Method *Find(const std::string &name) const {
        for (auto cls = this; cls; cls = cls->parent) {
            if (auto found = cls->methods.find(name); found != cls->methods.end())
                return &found->second;
        }
        return nullptr;
    }
};
struct Instance {
    Class *type;
    Environment fields;
};
struct ReturnValue {
    Value value;
};

class Runtime {
  public:
    Runtime(std::ostream &output, std::size_t depth_limit)
        : output_(output), depth_limit_(depth_limit) {}
    void ExecuteProgram(const Stmt &program) { Execute(program, globals_); }

  private:
    std::ostream &output_;
    const std::size_t depth_limit_;
    Environment globals_;
    std::unordered_map<std::string, Class *> named_classes_;
    // Value pointers are non-owning; stable arena objects are reclaimed together.
    std::vector<std::unique_ptr<Class>> classes_;
    std::vector<std::unique_ptr<Instance>> instances_;
    std::size_t depth_ = 0, steps_ = 0;
    void Step(std::size_t line) {
        if (++steps_ > kStepLimit)
            Fail(line, "execution step limit exceeded");
    }
    bool Truth(const Value &value) const {
        if (std::holds_alternative<std::monostate>(value))
            return false;
        if (auto p = std::get_if<bool>(&value))
            return *p;
        if (auto p = std::get_if<std::int64_t>(&value))
            return *p != 0;
        if (auto p = std::get_if<std::string>(&value))
            return !p->empty();
        return true;
    }
    Instance *Object(const Value &value, std::size_t line) const {
        if (auto p = std::get_if<Instance *>(&value))
            return *p;
        Fail(line, "expected class instance");
    }
    std::int64_t Number(const Value &value, std::size_t line) const {
        if (auto p = std::get_if<std::int64_t>(&value))
            return *p;
        Fail(line, "arithmetic requires integers");
    }
    bool Boolean(const Value &value, std::size_t line) const {
        if (auto p = std::get_if<bool>(&value))
            return *p;
        Fail(line, "comparison method must return bool");
    }
    Value Call(Instance *object, const std::string &name, const std::vector<Value> &arguments,
               std::size_t line) {
        DepthGuard guard(depth_, line, depth_limit_);
        Step(line);
        const auto *method = object->type->Find(name);
        if (!method)
            Fail(line, "method not found: " + name);
        if (method->parameters.size() != arguments.size())
            Fail(line, "wrong argument count for " + name);
        Environment local{{"self", object}};
        for (std::size_t i = 0; i < arguments.size(); ++i)
            local[method->parameters[i]] = arguments[i];
        try {
            Execute(method->body, local);
        } catch (const ReturnValue &result) {
            return result.value;
        }
        return std::monostate{};
    }
    std::string String(const Value &value, std::size_t line) {
        if (std::holds_alternative<std::monostate>(value))
            return "None";
        if (auto p = std::get_if<std::int64_t>(&value))
            return std::to_string(*p);
        if (auto p = std::get_if<bool>(&value))
            return *p ? "True" : "False";
        if (auto p = std::get_if<std::string>(&value))
            return *p;
        if (auto p = std::get_if<Class *>(&value))
            return "Class " + (*p)->name;
        auto object = std::get<Instance *>(value);
        if (!object->type->Find("__str__"))
            return "<" + object->type->name + " instance>";
        Value result = Call(object, "__str__", {}, line);
        if (auto p = std::get_if<std::string>(&result))
            return *p;
        Fail(line, "__str__ must return a string");
    }
    bool Equal(const Value &left, const Value &right, std::size_t line) {
        const bool left_none = std::holds_alternative<std::monostate>(left);
        const bool right_none = std::holds_alternative<std::monostate>(right);
        if (left_none || right_none)
            return left_none && right_none;
        if (auto object = std::get_if<Instance *>(&left))
            return Boolean(Call(*object, "__eq__", {right}, line), line);
        if (left.index() != right.index())
            Fail(line, "incompatible comparison operands");
        if (auto a = std::get_if<std::int64_t>(&left))
            return *a == std::get<std::int64_t>(right);
        if (auto a = std::get_if<std::string>(&left))
            return *a == std::get<std::string>(right);
        if (auto a = std::get_if<bool>(&left))
            return *a == std::get<bool>(right);
        Fail(line, "unsupported equality comparison");
    }
    bool Less(const Value &left, const Value &right, std::size_t line) {
        if (auto object = std::get_if<Instance *>(&left))
            return Boolean(Call(*object, "__lt__", {right}, line), line);
        if (left.index() != right.index())
            Fail(line, "incompatible comparison operands");
        if (auto a = std::get_if<std::int64_t>(&left))
            return *a < std::get<std::int64_t>(right);
        if (auto a = std::get_if<std::string>(&left))
            return *a < std::get<std::string>(right);
        if (auto a = std::get_if<bool>(&left))
            return *a < std::get<bool>(right);
        Fail(line, "unsupported ordering comparison");
    }
    Value Arithmetic(const std::string &op, const Value &left, const Value &right,
                     std::size_t line) {
        if (op == "+") {
            if (auto object = std::get_if<Instance *>(&left))
                return Call(*object, "__add__", {right}, line);
            if (auto a = std::get_if<std::string>(&left)) {
                auto b = std::get_if<std::string>(&right);
                if (!b)
                    Fail(line, "string addition requires another string");
                if (a->size() > kSourceLimit - b->size())
                    Fail(line, "string size limit exceeded");
                return *a + *b;
            }
        }
        const auto a = Number(left, line), b = Number(right, line);
        std::int64_t result = 0;
        bool overflow = false;
        if (op == "+")
            overflow = __builtin_add_overflow(a, b, &result);
        else if (op == "-")
            overflow = __builtin_sub_overflow(a, b, &result);
        else if (op == "*")
            overflow = __builtin_mul_overflow(a, b, &result);
        else {
            if (b == 0)
                Fail(line, "division by zero");
            if (a == std::numeric_limits<std::int64_t>::min() && b == -1)
                overflow = true;
            else
                result = a / b;
        }
        if (overflow)
            Fail(line, "integer arithmetic overflow");
        return result;
    }
    Value Evaluate(const Expr &expression, Environment &environment) {
        const auto line = expression->line;
        DepthGuard guard(depth_, line, depth_limit_);
        Step(line);
        switch (expression->type) {
        case Expression::Type::Literal:
            return expression->literal;
        case Expression::Type::Name: {
            if (auto it = environment.find(expression->text); it != environment.end())
                return it->second;
            if (auto it = named_classes_.find(expression->text); it != named_classes_.end())
                return it->second;
            Fail(line, "unknown variable: " + expression->text);
        }
        case Expression::Type::Attribute: {
            auto object = Object(Evaluate(expression->left, environment), line);
            auto it = object->fields.find(expression->text);
            if (it == object->fields.end())
                Fail(line, "unknown field: " + expression->text);
            return it->second;
        }
        case Expression::Type::Unary: {
            Value value = Evaluate(expression->left, environment);
            if (expression->text == "not")
                return !Truth(value);
            auto number = Number(value, line);
            if (expression->text == "+")
                return number;
            if (number == std::numeric_limits<std::int64_t>::min())
                Fail(line, "integer negation overflow");
            return -number;
        }
        case Expression::Type::Binary: {
            Value left = Evaluate(expression->left, environment);
            const auto &op = expression->text;
            if (op == "and")
                return Truth(left) && Truth(Evaluate(expression->right, environment));
            if (op == "or")
                return Truth(left) || Truth(Evaluate(expression->right, environment));
            Value right = Evaluate(expression->right, environment);
            if (op == "==")
                return Equal(left, right, line);
            if (op == "!=")
                return !Equal(left, right, line);
            if (op == "<")
                return Less(left, right, line);
            if (op == "<=")
                return Less(left, right, line) || Equal(left, right, line);
            if (op == ">")
                return !Less(left, right, line) && !Equal(left, right, line);
            if (op == ">=")
                return !Less(left, right, line);
            return Arithmetic(op, left, right, line);
        }
        case Expression::Type::Call: {
            const auto &callee = expression->left;
            // Evaluate the receiver before arguments; every argument is evaluated once, left to
            // right.
            Instance *receiver = nullptr;
            Value callable;
            const bool stringify = callee->type == Expression::Type::Name &&
                                   callee->text == "str" && !named_classes_.count("str");
            if (callee->type == Expression::Type::Attribute)
                receiver = Object(Evaluate(callee->left, environment), line);
            else if (!stringify)
                callable = Evaluate(callee, environment);
            std::vector<Value> arguments;
            for (const auto &argument : expression->arguments)
                arguments.push_back(Evaluate(argument, environment));
            if (receiver)
                return Call(receiver, callee->text, arguments, line);
            if (stringify) {
                if (arguments.size() != 1)
                    Fail(line, "str takes exactly one argument");
                return String(arguments.front(), line);
            }
            auto type = std::get_if<Class *>(&callable);
            if (!type)
                Fail(line, "only class constructors and methods are callable");
            if (instances_.size() >= 100000)
                Fail(line, "instance limit exceeded");
            auto instance = std::make_unique<Instance>();
            instance->type = *type;
            auto pointer = instance.get();
            instances_.push_back(std::move(instance));
            if ((*type)->Find("__init__"))
                Call(pointer, "__init__", arguments, line);
            else if (!arguments.empty())
                Fail(line, "constructor without __init__ takes no arguments");
            return pointer;
        }
        }
        Fail(line, "invalid expression");
    }
    void Execute(const Stmt &statement, Environment &environment) {
        DepthGuard guard(depth_, statement->line, depth_limit_);
        Step(statement->line);
        switch (statement->type) {
        case Statement::Type::Block:
            for (const auto &child : statement->statements)
                Execute(child, environment);
            return;
        case Statement::Type::Print: {
            // Render each argument in order; method side effects occur before its printed value.
            for (std::size_t i = 0; i < statement->arguments.size(); ++i) {
                auto value = Evaluate(statement->arguments[i], environment);
                auto text = String(value, statement->line);
                if (i)
                    output_ << ' ';
                output_ << text;
            }
            output_ << '\n';
            if (!output_)
                Fail(statement->line, "output write failed");
            return;
        }
        case Statement::Type::Assignment: {
            Value value = Evaluate(statement->expression, environment);
            if (statement->target->type == Expression::Type::Name)
                environment[statement->target->text] = std::move(value);
            else {
                auto object =
                    Object(Evaluate(statement->target->left, environment), statement->line);
                object->fields[statement->target->text] = std::move(value);
            }
            return;
        }
        case Statement::Type::Expression:
            Evaluate(statement->expression, environment);
            return;
        case Statement::Type::Return:
            throw ReturnValue{Evaluate(statement->expression, environment)};
        case Statement::Type::If:
            if (Truth(Evaluate(statement->expression, environment)))
                Execute(statement->body, environment);
            else if (statement->otherwise)
                Execute(statement->otherwise, environment);
            return;
        case Statement::Type::Class: {
            auto type = std::make_unique<Class>();
            type->name = statement->name;
            if (!statement->parent.empty()) {
                auto parent = named_classes_.find(statement->parent);
                if (parent == named_classes_.end())
                    Fail(statement->line, "base class definition has not executed");
                type->parent = parent->second;
            }
            for (const auto &method : statement->methods)
                type->methods.emplace(method.name, method);
            auto pointer = type.get();
            classes_.push_back(std::move(type));
            named_classes_[statement->name] = pointer;
            environment[statement->name] = pointer;
            return;
        }
        }
    }
};
} // namespace

void Run(std::istream &program, std::ostream &output) {
    Run(program, output, 256);
}

void Run(std::istream &program, std::ostream &output, std::size_t runtime_depth_limit) {
    if (runtime_depth_limit == 0 || runtime_depth_limit > kMaxRuntimeDepthLimit)
        throw Error("runtime depth limit must be in [1,2048]");
    Lexer lexer(program);
    Parser parser(lexer);
    const auto tree = parser.Program();
    Runtime runtime(output, runtime_depth_limit);
    runtime.ExecuteProgram(tree);
}
} // namespace museum::mython

#ifndef MYTHON_LIBRARY
int main() {
    try {
        museum::mython::Run(std::cin, std::cout, 2048);
        return 0;
    } catch (const museum::mython::Error &error) {
        std::cerr << "Mython: " << error.what() << '\n';
        return 1;
    } catch (const std::exception &error) {
        std::cerr << "Mython: " << error.what() << '\n';
        return 1;
    }
}
#endif
