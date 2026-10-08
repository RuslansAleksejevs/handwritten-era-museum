#include "constants.hpp"

namespace museum {

const int* local_from_other_file() noexcept { return &local_answer; }
const int* shared_from_other_file() noexcept { return &shared_answer; }
const char* title_from_other_file() noexcept { return title_view.data(); }

}  // namespace museum
