#pragma once

// These checks remain executable in every configuration. Visual Studio
// defines NDEBUG for Release by default, so the standard assert() macro is
// unsuitable for release validation.
#include <cstdio>
#include <cstdlib>

namespace erui::test {

[[noreturn]] inline void check_failed(
    const char* expression,
    const char* file,
    int line) noexcept {
    std::fprintf(
        stderr, "test check failed: %s (%s:%d)\n", expression, file, line);
    std::fflush(stderr);
    std::abort();
}

} // namespace erui::test

#define ERUI_TEST_CHECK(expression) \
    do { \
        if (!(expression)) { \
            ::erui::test::check_failed(#expression, __FILE__, __LINE__); \
        } \
    } while (false)
