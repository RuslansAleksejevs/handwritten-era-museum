#pragma once
#include <cstddef>
#include <cstdint>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace museum::mython {

class Error : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

enum class Kind {
    Number,
    Identifier,
    String,
    Symbol,
    Newline,
    Indent,
    Dedent,
    Eof,
    Class,
    Def,
    Return,
    If,
    Else,
    Print,
    Or,
    And,
    Not,
    None,
    True,
    False
};

struct Token {
    Kind kind;
    std::string text;
    std::int64_t number = 0;
    std::size_t line = 1;
    std::size_t column = 1;
};

// A two-space indentation lexer. Blank/comment-only lines produce no tokens.
class Lexer {
  public:
    explicit Lexer(std::istream &source);
    const Token &CurrentToken() const;
    const Token &NextToken();
    const Token &Expect(Kind kind) const;
    const Token &Expect(Kind kind, const std::string &text) const;
    const Token &ExpectNext(Kind kind);
    const Token &ExpectNext(Kind kind, const std::string &text);

  private:
    std::vector<Token> tokens_;
    std::size_t position_ = 0;
};

// Each call has a fresh environment/arena. Parse/runtime errors throw Error.
// Instances (including cyclic field graphs) remain alive until the call ends.
void Run(std::istream &program, std::ostream &output);

// The two-argument API keeps its conservative 256-frame runtime limit.
// Explicitly opt into 1..2048 frames only with sufficient native stack;
// the CLI uses 2048. This counts interpreter frames, not method calls.
void Run(std::istream &program, std::ostream &output, std::size_t runtime_depth_limit);

} // namespace museum::mython
