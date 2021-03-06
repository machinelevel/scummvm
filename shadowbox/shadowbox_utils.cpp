
#include <chrono>

namespace ShadowBoxNs {
static  std::chrono::time_point<std::chrono::high_resolution_clock> tic_start;

void TIC()
{
    tic_start = std::chrono::high_resolution_clock::now();
}

float TOC()
{
     std::chrono::time_point<std::chrono::high_resolution_clock> tic_stop = std::chrono::high_resolution_clock::now();
    long long int elapsed_usec = std::chrono::duration_cast<std::chrono::microseconds>(tic_stop - tic_start).count();
    float elapsed_ms = 0.001f * elapsed_usec;
    return elapsed_ms;
}

}
