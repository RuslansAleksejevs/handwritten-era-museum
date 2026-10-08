#pragma once
#include <string>
#include <vector>
namespace yellow::events {
enum class TokenKind { Word, Quoted, Operator, Left, Right };
struct Token {
    TokenKind kind;
    std::string text;
};
std::vector<Token> Tokenize(const std::string &);
} // namespace yellow::events
