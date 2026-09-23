#pragma once

// std
#include <cstdint>
#include <vector>
#include <optional>



namespace QB
{
    struct ClipboardImage
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        std::vector<uint8_t> Pixels;

        bool IsValid() const
        {
            return Width > 0 && Height > 0 && Pixels.size() == static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4;
        }
    };

    class Clipboard
    {
        public:
            static std::optional<ClipboardImage> GetImage();
    };

} // namespace QB