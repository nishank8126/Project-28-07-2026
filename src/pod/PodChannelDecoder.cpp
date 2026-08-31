#include "workstation/pod/PodChannelDecoder.h"

namespace workstation { namespace pod {

std::uint32_t NormalizeChannelCode(std::uint32_t value) {
    return (value >= 1 && value <= 7) ? value : 0;
}

std::uint32_t NormalizeFormatCode(std::uint32_t value) {
    return (value >= 1 && value <= 10) ? value : 0;
}

std::size_t GetElementSize(std::uint32_t type) {
    switch (type) {
        case 1:  return 4;
        case 2:  return 8;
        case 3:  return 0;   // confirmed: type 3 → 0 bytes
        case 4:  return 1;
        case 5:  return 1;
        case 6:  return 2;
        case 7:  return 2;
        case 8:  return 4;
        case 9:  return 4;
        case 10: return 8;
        default: return 0;
    }
}

bool checkedMultiply(std::size_t a, std::size_t b, std::size_t& result) {
    if (a != 0 && b > SIZE_MAX / a) return false;
    result = a * b;
    return true;
}

bool checkedAdd(std::size_t a, std::size_t b, std::size_t& result) {
    if (a > SIZE_MAX - b) return false;
    result = a + b;
    return true;
}

bool CalculatePayloadSize(std::uint32_t normalizedType, std::uint32_t count,
                          std::uint32_t stride, std::size_t& outBytes) {
    if (normalizedType == 0) return false;
    std::size_t elemSize = GetElementSize(normalizedType);
    std::size_t s_count = count;
    std::size_t s_stride = stride;
    std::size_t product;
    if (!checkedMultiply(elemSize, s_count, product)) return false;
    if (!checkedMultiply(product, s_stride, outBytes)) return false;
    return true;
}

} // namespace pod
} // namespace workstation
