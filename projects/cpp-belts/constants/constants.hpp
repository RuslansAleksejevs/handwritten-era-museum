#pragma once

#include <string_view>

namespace museum {

// A namespace-scope, non-extern const object has internal linkage.
const int local_answer = 42;

// C++17: every translation unit refers to the same inline variable.
inline constexpr int shared_answer = 42;

// The view borrows storage from an array with static storage duration.
inline constexpr char title[] = "A Personal Museum of Code";
inline constexpr std::string_view title_view{title};

const int* local_from_other_file() noexcept;
const int* shared_from_other_file() noexcept;
const char* title_from_other_file() noexcept;

}  // namespace museum
