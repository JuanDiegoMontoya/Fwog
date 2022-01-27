#pragma once

// Taken from https://github.com/cdgiessen/vk-module/blob/076baa98cba35cd93a6ab56c3fd1b1ea2313f806/codegen_text.py#L53
// Thanks Thomas!
#define DECLARE_FLAG_TYPE(FLAG_TYPE, FLAG_BITS, BASE_TYPE)                                 \
                                                                                           \
struct FLAG_TYPE {                                                                         \
    BASE_TYPE flags = static_cast<BASE_TYPE>(0);                                           \
                                                                                           \
    constexpr FLAG_TYPE() noexcept = default;                                              \
    constexpr explicit FLAG_TYPE(BASE_TYPE in) noexcept: flags(in){ }                      \
    constexpr FLAG_TYPE(FLAG_BITS in) noexcept: flags(static_cast<BASE_TYPE>(in)){ }       \
    constexpr bool operator==(FLAG_TYPE const& right) const { return flags == right.flags;}\
    constexpr bool operator!=(FLAG_TYPE const& right) const { return flags != right.flags;}\
    constexpr explicit operator BASE_TYPE() const { return flags;}                         \
    constexpr explicit operator bool() const noexcept {                                    \
      return flags != 0;                                                                   \
    }                                                                                      \
};                                                                                         \
constexpr FLAG_TYPE operator|(FLAG_TYPE a, FLAG_TYPE b) noexcept {                         \
    return static_cast<FLAG_TYPE>(a.flags | b.flags);                                      \
}                                                                                          \
constexpr FLAG_TYPE operator&(FLAG_TYPE a, FLAG_TYPE b) noexcept {                         \
    return static_cast<FLAG_TYPE>(a.flags & b.flags);                                      \
}                                                                                          \
constexpr FLAG_TYPE operator^(FLAG_TYPE a, FLAG_TYPE b) noexcept {                         \
    return static_cast<FLAG_TYPE>(a.flags ^ b.flags);                                      \
}                                                                                          \
constexpr FLAG_TYPE operator~(FLAG_TYPE a) noexcept {                                      \
    return static_cast<FLAG_TYPE>(~a.flags);                                               \
}                                                                                          \
constexpr FLAG_TYPE& operator|=(FLAG_TYPE& a, FLAG_TYPE b) noexcept {                      \
    a.flags = (a.flags | b.flags);                                                         \
    return a;                                                                              \
}                                                                                          \
constexpr FLAG_TYPE& operator&=(FLAG_TYPE& a, FLAG_TYPE b) noexcept {                      \
    a.flags = (a.flags & b.flags);                                                         \
    return a;                                                                              \
}                                                                                          \
constexpr FLAG_TYPE operator^=(FLAG_TYPE& a, FLAG_TYPE b) noexcept {                       \
    a.flags = (a.flags ^ b.flags);                                                         \
    return a;                                                                              \
}                                                                                          \
constexpr FLAG_TYPE operator|(FLAG_BITS a, FLAG_BITS b) noexcept {                         \
    return static_cast<FLAG_TYPE>(static_cast<BASE_TYPE>(a) | static_cast<BASE_TYPE>(b));  \
}                                                                                          \
constexpr FLAG_TYPE operator&(FLAG_BITS a, FLAG_BITS b) noexcept {                         \
    return static_cast<FLAG_TYPE>(static_cast<BASE_TYPE>(a) & static_cast<BASE_TYPE>(b));  \
}                                                                                          \
constexpr FLAG_TYPE operator~(FLAG_BITS key) noexcept {                                    \
    return static_cast<FLAG_TYPE>(~static_cast<BASE_TYPE>(key));                           \
}                                                                                          \
constexpr FLAG_TYPE operator^(FLAG_BITS a, FLAG_BITS b) noexcept {                         \
    return static_cast<FLAG_TYPE>(static_cast<BASE_TYPE>(a) ^ static_cast<BASE_TYPE>(b));  \
}
