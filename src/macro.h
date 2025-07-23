#pragma once
#include <string.h>
#include <assert.h>
#include "util.h"


#if defined __GNUC__ || defined __llvm__
#define TINYTCP_LICKLY(x) __builtin_expect(!!(x), 1)
#define TINYTCP_UNLICKLY(x) __builtin_expect(!!(x), 0)
#else
#define TINYTCP_LICKLY(x)   (x)
#define TINYTCP_UNLICKLY(x) (x)
#endif


#define TINYTCP_ASSERT(x) \
    if (TINYTCP_UNLICKLY(!(x))) { \
        TINYTCP_LOG_ERROR(TINYTCP_LOG_ROOT()) << "ASSERTION: " #x \
            << "\nbacktrace:\n" \
            << tinytcp::back_trace_to_string(100, 2, "    "); \
        assert(x); \
    }

#define TINYTCP_ASSERT2(x, w) \
    if (TINYTCP_UNLICKLY(!(x))) { \
        TINYTCP_LOG_ERROR(TINYTCP_LOG_ROOT()) << "ASSERTION: " #x \
            << "\n" << w \
            << "\nbacktrace:\n" \
            << tinytcp::back_trace_to_string(100, 2, "    "); \
        assert(x); \
    }

