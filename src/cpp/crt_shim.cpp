// UCRT has no sincosf, yet LLVM folds a sin(x)/cos(x) pair into a call to it.
// Defining the symbol here keeps the Zig core portable without special compiler flags.
#include <cmath>

#if defined(_MSC_VER)

extern "C" void sincosf(float radians, float* sine, float* cosine) {
    *sine = std::sin(radians);
    *cosine = std::cos(radians);
}

#endif
