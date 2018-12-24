#pragma	once

#include <stdint.h>

#ifdef _WIN32
inline uint16_t bswap_16(uint16_t x)
{
    return (x >> 8) | ((x & 0x00ff) << 8);
}

inline uint32_t bswap_32(uint32_t x)
{
    return (((x & 0xff000000U) >> 24) | ((x & 0x00ff0000U) >>  8) |
            ((x & 0x0000ff00U) <<  8) | ((x & 0x000000ffU) << 24));
}

inline uint64_t bswap_64(uint64_t x)
{
     return (((x & 0xff00000000000000ull) >> 56)
          | ((x & 0x00ff000000000000ull) >> 40)
          | ((x & 0x0000ff0000000000ull) >> 24)
          | ((x & 0x000000ff00000000ull) >> 8)
          | ((x & 0x00000000ff000000ull) << 8)
          | ((x & 0x0000000000ff0000ull) << 24)
          | ((x & 0x000000000000ff00ull) << 40)
          | ((x & 0x00000000000000ffull) << 56));
}

inline uint32_t be32toh(uint32_t big_endian_32bits)
{
    return bswap_32(big_endian_32bits);
}

inline uint32_t htole32(uint32_t lit_endian_32bits)
{
	return lit_endian_32bits;
}

inline uint32_t htobe32(uint32_t host_32bits)
{
	return bswap_32(host_32bits);
}

inline uint64_t htobe64(uint64_t host_64bits)
{
	return bswap_64(host_64bits);
}
#endif
