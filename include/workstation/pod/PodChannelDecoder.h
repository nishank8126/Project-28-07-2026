#pragma once
#include <cstddef>
#include <cstdint>

namespace workstation { namespace pod {

// ---- Normalization functions (confirmed, separate per spec) ----

// Channel-code normalization (FUN_18006e560): 1..7 → valid, else 0.
std::uint32_t NormalizeChannelCode(std::uint32_t value);

// Format-code normalization (FUN_18006e630): 1..10 → valid, else 0.
std::uint32_t NormalizeFormatCode(std::uint32_t value);

// Exact element-size table (FUN_18006e5d0). DO NOT alter.
std::size_t GetElementSize(std::uint32_t type);

// ---- Checked arithmetic (overflow protection) ----

bool checkedMultiply(std::size_t a, std::size_t b, std::size_t& result);
bool checkedAdd(std::size_t a, std::size_t b, std::size_t& result);

// ---- Payload-size calculation ----

// payloadBytes = GetElementSize(normalizedType) * count * stride
bool CalculatePayloadSize(std::uint32_t normalizedType, std::uint32_t count,
                          std::uint32_t stride, std::size_t& outBytes);

} // namespace pod
} // namespace workstation
