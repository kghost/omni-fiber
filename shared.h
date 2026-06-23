#pragma once

#if defined(_MSC_VER) && defined(OMNIFIBER_DYNAMIC)

#ifdef OMNIFIBER_EXPORTS
#define OMNIFIBER_API __declspec(dllexport)
#else
#define OMNIFIBER_API __declspec(dllimport)
#endif

#elif defined(__GNUC__) && defined(OMNIFIBER_DYNAMIC)

#ifdef OMNIFIBER_EXPORTS
#define OMNIFIBER_API __attribute__((visibility("default")))
#else
#define OMNIFIBER_API
#endif

#else
#define OMNIFIBER_API
#endif
