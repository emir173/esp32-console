#pragma once
#include <stddef.h>
#include <stdint.h>

struct BmpLayout {
    uint32_t dataOffset;
    uint32_t rowSize;
    bool bottomUp;
};

inline uint16_t bmpU16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

inline uint32_t bmpU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Desteklenen format: sıkıştırılmamış 24-bit Windows BMP, iki satır yönü.
// Tüm sınırlar doğrulanmadan piksel tamponuna dokunulmaz.
inline bool decodeBMP24(const uint8_t* data, size_t size, int width, int height,
                        BmpLayout &layout) {
    if (!data || size < 54 || width <= 0 || height <= 0) return false;
    if (data[0] != 'B' || data[1] != 'M') return false;
    uint32_t dibSize = bmpU32(data + 14);
    uint32_t offset = bmpU32(data + 10);
    if (dibSize < 40 || dibSize > size - 14 || offset < 14 + dibSize || offset > size)
        return false;
    if (bmpU32(data + 18) != (uint32_t)width) return false;
    uint32_t storedHeight = bmpU32(data + 22);
    if (storedHeight != (uint32_t)height && storedHeight != (uint32_t)(-height)) return false;
    if (bmpU16(data + 26) != 1 || bmpU16(data + 28) != 24 || bmpU32(data + 30) != 0)
        return false;
    uint32_t rowSize = ((uint32_t)width * 3 + 3) & ~3u;
    if ((size - offset) / rowSize < (uint32_t)height) return false;
    layout.dataOffset = offset;
    layout.rowSize = rowSize;
    layout.bottomUp = storedHeight == (uint32_t)height;
    return true;
}
