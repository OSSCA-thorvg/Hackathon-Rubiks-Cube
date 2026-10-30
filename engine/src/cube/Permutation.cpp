#include "cube/Permutation.hpp"

namespace rubiks::cube {

std::vector<int> orbits(const std::vector<std::vector<int>>& steps,
                        std::size_t count)
{
    std::vector<int> which(count, -1);
    int next = 0;
    for (std::size_t start = 0; start < which.size(); ++start) {
        if (which[start] >= 0) continue;

        which[start] = next;
        std::vector<int> waiting{static_cast<int>(start)};
        while (!waiting.empty()) {
            const auto here = static_cast<std::size_t>(waiting.back());
            waiting.pop_back();
            for (const auto& lands : steps) {
                const auto there = static_cast<std::size_t>(lands[here]);
                if (which[there] >= 0) continue;
                which[there] = next;
                waiting.push_back(static_cast<int>(there));
            }
        }
        ++next;
    }
    return which;
}

bool is_odd_permutation(const std::vector<int>& permutation)
{
    // Every cycle of even length is an odd number of swaps.
    std::vector<bool> seen(permutation.size(), false);
    bool odd = false;
    for (std::size_t i = 0; i < permutation.size(); ++i) {
        if (seen[i]) continue;
        std::size_t length = 0;
        for (std::size_t at = i; !seen[at];
             at = static_cast<std::size_t>(permutation[at])) {
            seen[at] = true;
            ++length;
        }
        if (length % 2 == 0) odd = !odd;
    }
    return odd;
}

}  // namespace rubiks::cube
