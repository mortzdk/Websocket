#pragma once

#ifndef wss_core_h
#define wss_core_h

#if (__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))
#define WSS_OS_UNIX
#endif // UNIX

#ifdef __linux__
#define WSS_OS_LINUX
#endif // __linux__

#ifdef _WIN32
#define WSS_OS_WINDOWS
#endif // __WIN32__

#ifdef __APPLE__
#define WSS_OS_MACOS
#endif // __APPLE__

#ifdef __FreeBSD__
#define WSS_OS_FREEBSD
#endif // __FreeBSD__

#ifdef __NetBSD__
#define WSS_OS_NETBSD
#endif // __NetBSD__

#ifdef __OpenBSD__
#define WSS_OS_OPENBSD
#endif // __OpenBSD__

#if defined(WSS_OS_LINUX) || defined(WSS_OS_MACOS) || defined(WSS_OS_FREEBSD)
#if __has_builtin(__builtin_debugtrap)
#define WSS_BREAKPOINT() __builtin_debugtrap()
#elif __has_builtin(__builtin_trap)
#define WSS_BREAKPOINT() __builtin_trap()
#else
#include <signal.h>
#if defined(SIGTRAP)
#define WSS_BREAKPOINT() raise(SIGTRAP)
#else
#define WSS_BREAKPOINT() raise(SIGABRT)
#endif // defined(SIGTRAP)
#endif // __has_builtin(__builtin_debugtrap)
#elif defined (WSS_OS_WINDOWS)
#define WSS_BREAKPOINT() __debugbreak()
#endif // defined(WSS_OS_LINUX)
       
#if _WIN32 || _WIN64
#if _WIN64
#define CORE_ARCH_X86_64
#else
#error "32-bit windows is not support"
#endif
#endif

#if defined(__x86_64__)
#define WSS_ARCH_X86_64
#endif

#if defined(__aarch64__)
#define WSS_ARCH_AARCH64
#endif

#if defined(__arm__)
#define WSS_ARCH_ARM
#endif

#if defined(__GNUC__ ) || defined(__INTEL_COMPILER)
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#else
#define likely(x)      (x)
#define unlikely(x)    (x)
#endif

#ifndef alignof
#define alignof _Alignof
#endif

#ifndef alignas
#define alignas _Alignas
#endif

#ifndef thread_local
#define thread_local _Thread_local
#endif

#ifndef bool
#define bool _Bool
#endif

#ifndef noreturn
#define noreturn _Noreturn
#endif

#ifndef visible
#define visible __attribute__ ((visibility("default")))
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#ifndef fallthrough
#define fallthrough __attribute__ ((fallthrough))
#endif

#define WSS_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define WSS_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define WSS_CLAMP(v, min, max) (((v) > (max)) ? (max) : ((v) < (min)) ? (min) : (v))
#define WSS_UNUSED(expr) ((void)(expr))

#define WSS_POINTER_ADD(ptr, by) (void*)((uintptr_t)(ptr) + (uintptr_t)(by))
#define WSS_POINTER_SUB(ptr, by) (void*)((uintptr_t)(ptr) - (uintptr_t)(by))

#if defined(WSS_OS_LINUX) && !defined(USE_POLL)
#define WSS_EPOLL 1
#elif defined(WSS_OS_UNIX) && !defined(USE_POLL)
#define WSS_KQUEUE 1
#elif defined(WSS_OS_WINDOWS)
#error "Windows is not support"
#else
#define WSS_POLL 1
#endif

#endif
