#include <deque>

namespace test {

void for_each_loop(std::deque<int> const& deq, int& r) {
    for (auto i : deq) {
        r += i;
    }
}


}