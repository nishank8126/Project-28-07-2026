#pragma once
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace workstation { namespace pod {

// Byte-key for handler registry. Comparison follows recovered FUN_18000B940
// semantics: memcmp on common prefix, shorter key sorts before longer key.
class HandlerKey {
public:
    HandlerKey() = default;
    HandlerKey(std::vector<std::uint8_t> data) : data_(std::move(data)) {}
    HandlerKey(std::initializer_list<std::uint8_t> il) : data_(il) {}

    const std::vector<std::uint8_t>& data() const { return data_; }
    std::size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }
    const std::uint8_t* bytes() const { return data_.data(); }

    // Recovered comparison (FUN_18000B940).
    // memcmp on common prefix; shorter < longer when common bytes equal.
    bool operator<(const HandlerKey& other) const {
        return KeyLess(data_, other.data_);
    }

    bool operator==(const HandlerKey& other) const {
        return data_ == other.data_;
    }

    // Recovered byte-key comparison.
    static bool KeyLess(std::span<const std::uint8_t> a,
                        std::span<const std::uint8_t> b) {
        std::size_t n = a.size() < b.size() ? a.size() : b.size();
        int c = std::memcmp(a.data(), b.data(), n);
        if (c != 0) return c < 0;
        return a.size() < b.size();
    }

private:
    std::vector<std::uint8_t> data_;
};

}} // namespace workstation::pod
