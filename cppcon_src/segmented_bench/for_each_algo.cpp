#include <algorithm>
#include <deque>

namespace test {

void for_each_algo(std::deque<int> const& deq, int& r) {
    std::ranges::for_each(deq, [&r](int i) {
        r += i;
    });
}


}