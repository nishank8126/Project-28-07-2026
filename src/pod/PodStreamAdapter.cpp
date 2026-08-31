#include "workstation/pod/PodStreamAdapter.h"
#include <algorithm>

namespace workstation { namespace pod {

PodStreamAdapter::PodStreamAdapter(PodDataSource& source)
    : source_(source), buffer_(WORKING_BUFFER_SIZE) {}

bool PodStreamAdapter::fixedRead(std::size_t n) {
    if (n == 0) return true;

    // If the working buffer already has enough data, just advance.
    if (end_ - offset_ >= n) {
        return true;
    }

    // Clear the working buffer and load fresh data.
    offset_ = 0;
    end_ = 0;

    // Read exactly n bytes from source.
    while (end_ < n) {
        std::size_t want = buffer_.size() - end_;
        std::size_t got = source_.readSome(&buffer_[end_], want);
        if (got == 0) return false; // EOF or error
        end_ += got;
        accumulated_ += got;
    }

    return true;
}

std::size_t PodStreamAdapter::transfer(std::size_t maxBytes) {
    // If buffer already has data, return what we have (up to maxBytes).
    std::size_t available = end_ - offset_;
    if (available > 0) {
        std::size_t ret = available < maxBytes ? available : maxBytes;
        return ret;
    }

    // Buffer is empty; refill from source.
    offset_ = 0;
    end_ = 0;

    std::size_t got = source_.readSome(buffer_.data(), buffer_.size());
    if (got == 0) return 0;
    end_ += got;
    accumulated_ += got;

    std::size_t ret = end_ < maxBytes ? end_ : maxBytes;
    return ret;
}

void PodStreamAdapter::consume(std::size_t bytes) {
    std::size_t avail = end_ - offset_;
    if (bytes >= avail) {
        offset_ = 0;
        end_ = 0;
    } else {
        offset_ += bytes;
    }
}

}} // namespace workstation::pod
