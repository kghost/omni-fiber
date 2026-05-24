#pragma once

#if defined(_MSC_VER)

#ifdef OMNIFIBER_EXPORTS
#define OMNIFIBER_API __declspec(dllexport)
#else
#define OMNIFIBER_API __declspec(dllimport)
#endif

#elif defined(__GNUC__)

#ifdef OMNIFIBER_EXPORTS
#define OMNIFIBER_API __attribute__((visibility("default")))
#else
#define OMNIFIBER_API
#endif

#else
#pragma warning Unknown dynamic link import / export semantics.
#endif
