#include "sum_reverse_sort.h"
#include <algorithm>
namespace yellow {
int Sum(int a, int b) { return a + b; }
std::string Reverse(std::string s) {
    std::reverse(s.begin(), s.end());
    return s;
}
void Sort(std::vector<int> &v) { std::sort(v.begin(), v.end()); }
} // namespace yellow
