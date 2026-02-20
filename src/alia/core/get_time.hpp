#ifndef GET_TIME_D5ACFFE4_EEB1_40ED_8A6F_D6C991FC642D
#define GET_TIME_D5ACFFE4_EEB1_40ED_8A6F_D6C991FC642D

#include <chrono>

namespace alia {

inline double get_time() {
    using clock = std::chrono::steady_clock;
    static const clock::time_point start = clock::now();
    auto now = clock::now();
    auto duration = std::chrono::duration<double>(now - start);
    return duration.count();
}

} // namespace alia

#endif /* GET_TIME_D5ACFFE4_EEB1_40ED_8A6F_D6C991FC642D */
