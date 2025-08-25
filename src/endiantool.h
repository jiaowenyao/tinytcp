#pragma once

#define TINYTCP_LITTLE_ENDIAN 1
#define TINYTCP_BIG_ENDIAN 2


#include <stdint.h>
#include <type_traits>

namespace tinytcp {

static uint64_t bswap_64(uint64_t value) {
    return ((value & 0xFF00000000000000ULL) >> 56) |
           ((value & 0x00FF000000000000ULL) >> 40) |
           ((value & 0x0000FF0000000000ULL) >> 24) |
           ((value & 0x000000FF00000000ULL) >> 8)  |
           ((value & 0x00000000FF000000ULL) << 8)  |
           ((value & 0x0000000000FF0000ULL) << 24) |
           ((value & 0x000000000000FF00ULL) << 40) |
           ((value & 0x00000000000000FFULL) << 56);
}

static uint32_t bswap_32(uint32_t value) {
    return ((value & 0xFF000000) >> 24) |
           ((value & 0x00FF0000) >> 8)  |
           ((value & 0x0000FF00) << 8)  |
           ((value & 0x000000FF) << 24);
}

static uint16_t bswap_16(uint16_t value) {
    return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
}

template<class T>
typename std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type
byteswap(T value) {
    return (T)bswap_64((uint64_t)value);
}

template<class T>
typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type
byteswap(T value) {
    return (T)bswap_32((uint32_t)value);
}

template<class T>
typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type
byteswap(T value) {
    return (T)bswap_16((uint16_t)value);
}

#if BYTE_ORDER == BIG_ENDIAN
#define TINYTCP_BYTE_ORDER TINYTCP_BIG_ENDIAN
#else
#define TINYTCP_BYTE_ORDER TINYTCP_LITTLE_ENDIAN
#endif

#if TINYTCP_BYTE_ORDER == TINYTCP_BIG_ENDIAN

template<class T>
T host_to_net(T t) {
    return t;
}

template<class T>
T net_to_host(T t) {
    return t;
}

#else

template<class T>
T host_to_net(T t) {
    return byteswap(t);
}

template<class T>
T net_to_host(T t) {
    return byteswap(t);
}
#endif

}

