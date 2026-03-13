/**
 * @file base.hpp
 * @brief Common configurations, macros, and forward declarations.
 */
#ifndef YADAAC_BASE_HPP
#define YADAAC_BASE_HPP

#include <cstddef>

#if defined(__GNUC__) || defined(__clang__)
#  define YADAAC_INLINE inline __attribute__((always_inline))
#  define YADAAC_LIKELY(x) __builtin_expect(!!(x), 1)
#  define YADAAC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#elif defined(_MSC_VER)
#  define YADAAC_INLINE __forceinline
#  define YADAAC_LIKELY(x) (x)
#  define YADAAC_UNLIKELY(x) (x)
#else
#  define YADAAC_INLINE inline
#  define YADAAC_LIKELY(x) (x)
#  define YADAAC_UNLIKELY(x) (x)
#endif

namespace yadaac {

constexpr size_t MAX_PATTERN_AMOUNT = 0x00FFFFFF;
constexpr size_t ALPHABET_SIZE = 256;

}  // namespace yadaac

#endif  // YADAAC_BASE_HPP
