#pragma once

// Weird Engine runtime assertions.
//
// Enabled when the WEIRD_ENABLE_ASSERTS compile definition is set (done by the
// engine's CMake for Debug and RelWithDebInfo builds by default). These remain
// active in RelWithDebInfo even though CMake defines NDEBUG there, which is
// why this is not a wrapper around the standard assert().
//
// When disabled, the macro compiles away entirely and imposes zero overhead.
#if defined(WEIRD_ENABLE_ASSERTS)

#include <cstdio>
#include <cstdlib>

#define WEIRD_ASSERT(condition, message)                                                                               \
	do                                                                                                                 \
	{                                                                                                                  \
		if (!(condition))                                                                                              \
		{                                                                                                              \
			std::fprintf(stderr, "WEIRD ASSERT FAILED: %s\n  %s\n  at %s:%d\n", #condition, message, __FILE__,         \
						 __LINE__);                                                                                    \
			std::abort();                                                                                              \
		}                                                                                                              \
	} while (false)

#else

#define WEIRD_ASSERT(condition, message) ((void)0)

#endif
