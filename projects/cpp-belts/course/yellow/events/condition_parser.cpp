#include "condition_parser.h"
#include "token.h"
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
namespace yellow::events {
namespace {
class Parser {
    std::vector<Token> tokens_;
    std::size_t position_ = 0;
    bool TakeWord(const std::string &word) {
        if (position_ < tokens_.size() && tokens_[position_].kind == TokenKind::Word &&
            tokens_[position_].text == word) {
            ++position_;
            return true;
        }
        return false;
    }
    Token Take(TokenKind kind) {
        if (position_ == tokens_.size() || tokens_[position_].kind != kind)
            throw std::invalid_argument("unexpected condition token");
        return tokens_[position_++];
    }
    std::shared_ptr<Node> Primary() {
        if (position_ < tokens_.size() && tokens_[position_].kind == TokenKind::Left) {
            ++position_;
            auto n = Or();
            Take(TokenKind::Right);
            return n;
        }
        auto field = Take(TokenKind::Word).text;
        auto op = Take(TokenKind::Operator).text;
        static const std::map<std::string, Comparison> operators = {
            {"<", Comparison::Less},    {"<=", Comparison::LessOrEqual},
            {">", Comparison::Greater}, {">=", Comparison::GreaterOrEqual},
            {"==", Comparison::Equal},  {"!=", Comparison::NotEqual}};
        auto found = operators.find(op);
        if (found == operators.end())
            throw std::invalid_argument("unknown comparison");
        if (field == "date") {
            std::istringstream value(Take(TokenKind::Word).text);
            return std::make_shared<DateComparisonNode>(found->second, ParseDate(value));
        }
        if (field == "event")
            return std::make_shared<EventComparisonNode>(found->second,
                                                         Take(TokenKind::Quoted).text);
        throw std::invalid_argument("unknown condition field");
    }
    std::shared_ptr<Node> And() {
        auto result = Primary();
        while (TakeWord("AND"))
            result =
                std::make_shared<LogicalOperationNode>(LogicalOperation::And, result, Primary());
        return result;
    }
    std::shared_ptr<Node> Or() {
        auto result = And();
        while (TakeWord("OR"))
            result = std::make_shared<LogicalOperationNode>(LogicalOperation::Or, result, And());
        return result;
    }

  public:
    explicit Parser(std::vector<Token> t) : tokens_(std::move(t)) {}
    std::shared_ptr<Node> Parse() {
        if (tokens_.empty())
            return std::make_shared<EmptyNode>();
        auto result = Or();
        if (position_ != tokens_.size())
            throw std::invalid_argument("trailing condition tokens");
        return result;
    }
};
} // namespace
std::shared_ptr<Node> ParseCondition(std::istream &in) {
    std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    return Parser(Tokenize(text)).Parse();
}
} // namespace yellow::events
