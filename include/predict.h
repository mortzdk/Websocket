#ifndef wss_predict_h
#define wss_predict_h

#if defined(__GNUC__ ) || defined(__INTEL_COMPILER)
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#else
#define likely(x)      (x)
#define unlikely(x)    (x)
#endif

#endif
