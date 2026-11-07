#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace content {
// Streaming SHA-256, used for content identity, not authentication.
class Sha256 {
public:
    Sha256();
    void update(const void* data, size_t size);
    std::string finish();
private:
    void block(const unsigned char* data);
    uint32_t state_[8];
    uint64_t bytes_ = 0;
    unsigned char buffer_[64] = {};
    size_t used_ = 0;
    bool finished_ = false;
    std::string result_;
};
}
