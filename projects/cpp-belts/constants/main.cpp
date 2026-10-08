#include "constants.hpp"

#include <iostream>

static_assert(museum::local_answer == 42);
static_assert(museum::shared_answer == 42);
static_assert(museum::title_view.size() == 25);

int main() {
    // Check identity across separately compiled source files, not just values.
    const bool separate_local_objects =
        &museum::local_answer != museum::local_from_other_file();
    const bool shared_inline_object =
        &museum::shared_answer == museum::shared_from_other_file();
    const bool shared_string_storage =
        museum::title_view.data() == museum::title_from_other_file();

    if (!separate_local_objects || !shared_inline_object || !shared_string_storage) {
        std::cerr << "FAIL: unexpected cross-file object identity\n";
        return 1;
    }
    std::cout << "PASS: namespace const has distinct objects; inline constexpr "
                 "shares one; string_view refers to static array storage.\n";
}
