#pragma once

#if defined(_WIN32) && defined(NT_CONSOLE_APP)
#define NT_API
#elif defined(_WIN32) && defined(NT_EXPORTS)
#define NT_API __declspec(dllexport)
#elif defined(_WIN32)
#define NT_API __declspec(dllimport)
#else
#define NT_API
#endif
