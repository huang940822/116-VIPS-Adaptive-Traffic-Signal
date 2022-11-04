#ifndef UTIL_H
#define UTIL_H
#if !(defined(__CHAR_BIT__) && defined(__SIZEOF_LONG__))
#error Missing required predefined macros for BITS_PER_LONG calculation
#endif

#define BITS_PER_LONG (__CHAR_BIT__ * __SIZEOF_LONG__)

#define GEN_FRACTION_MASK(hpos, lpos) \
    (((~0UL) - (1UL << (lpos)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (hpos))))

#define BIT(n) (1UL << (n))
#define BIT_MASK(n) (BIT(n) - 1)

#define WRITE_BIT(var, bit, bool_set_or_clear) \
    ((var) = (bool_set_or_clear) ? ((var) | BIT(bit)) : ((var) & ~BIT(bit)))

#define SET_BIT(var, bit) (WRITE_BIT(var, bit, 1UL))

#define CLEAR_BIT(var, bit) (WRITE_BIT(var, bit, 0UL))

#define Malloc(obj, len, error_log) \
    obj = malloc(len); \
    if (obj == NULL) { \
        set_memory_error(); \
        log_file_write_fatal_error(error_log": malloc"); \
        perror(error_log": malloc"); \
        exit(errno); \
    } else { \
        clear_memory_error(); \
        memset(obj, 0, len); \
    }

// Close debug mode on deployment
#define DEBUG_MOD 1
#if DEBUG_MOD
#define printf(...) printf(__VA_ARGS__)
#else
#define printf(...) ;
#endif

#endif