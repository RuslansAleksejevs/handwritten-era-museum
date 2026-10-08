#include "token.h"
#include <cctype>
#include <stdexcept>
namespace yellow::events {
std::vector<Token> Tokenize(const std::string &input) {
    std::vector<Token> result;
    std::size_t p = 0;
    while (p < input.size()) {
        const char c = input[p];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++p;
            continue;
        }
        if (c == '(' || c == ')') {
            result.push_back({c == '(' ? TokenKind::Left : TokenKind::Right, std::string(1, c)});
            ++p;
            continue;
        }
        if (c == '"') {
            const auto end = input.find('"', p + 1);
            if (end == input.npos)
                throw std::invalid_argument("unterminated event literal");
            result.push_back({TokenKind::Quoted, input.substr(p + 1, end - p - 1)});
            p = end + 1;
            continue;
        }
        if (std::string("<>=!").find(c) != std::string::npos) {
            std::string op(1, c);
            ++p;
            if (p < input.size() && input[p] == '=') {
                op += '=';
                ++p;
            }
            result.push_back({TokenKind::Operator, std::move(op)});
            continue;
        }
        const auto begin = p;
        while (p < input.size() && !std::isspace(static_cast<unsigned char>(input[p])) &&
               std::string("()\"<>=!").find(input[p]) == std::string::npos)
            ++p;
        result.push_back({TokenKind::Word, input.substr(begin, p - begin)});
    }
    return result;
}
} // namespace yellow::events
