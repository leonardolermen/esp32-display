#pragma once

#include <cstddef>
#include <optional>
#include <string>

class UploadCheck {
public:
    static constexpr size_t MAX_PHOTO_BYTES = 200 * 1024;
    static constexpr size_t MIN_FREE_BYTES = 512 * 1024;
    static constexpr size_t MAX_PHOTOS = 200;

    static std::optional<std::string> refuse(size_t declaredBytes, size_t freeBytes, size_t photoCount);
    static std::optional<std::string> refuseReceived(size_t receivedBytes);
};
