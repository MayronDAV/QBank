#include "Clipboard.h"

#if defined(QB_WINDOWS)

#include <Windows.h>

#include <algorithm>
#include <cstring>

namespace QB
{
    namespace
    {
        size_t GetStride(int width, WORD bitsPerPixel)
        {
            return ((static_cast<size_t>(width) * bitsPerPixel + 31ull) / 32ull) * 4ull;
        }

        std::optional<ClipboardImage> ConvertDIB(const void* dibData, size_t dibSize)
        {
            if (!dibData || dibSize < sizeof(BITMAPINFOHEADER))
                return std::nullopt;

            const uint8_t* bytes           = static_cast<const uint8_t*>(dibData);
            const BITMAPINFOHEADER* header = reinterpret_cast<const BITMAPINFOHEADER*>(bytes);

            if (header->biSize < sizeof(BITMAPINFOHEADER))
                return std::nullopt;

            if (header->biSize > dibSize)
                return std::nullopt;

            const int width      = header->biWidth;
            if (width <= 0)
                return std::nullopt;

            const bool bottomUp = header->biHeight > 0;
            const int height    = header->biHeight == INT_MIN ? 0 : std::abs(header->biHeight);

            if (height <= 0)
                return std::nullopt;

            const WORD bpp = header->biBitCount;
            if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32)
            {
                return std::nullopt;
            }

            const size_t stride      = GetStride(width, bpp);
            size_t colorTableOffset  = header->biSize;
            size_t colorTableEntries = 0;

            if (bpp <= 8)
            {
                if (header->biClrUsed != 0)
                    colorTableEntries = header->biClrUsed;
                else
                    colorTableEntries = 1ull << bpp;
            }

            size_t masksSize  = 0;
            if (header->biCompression == BI_BITFIELDS && bpp == 32)
            {
                if (header->biSize == sizeof(BITMAPINFOHEADER))
                    masksSize = sizeof(DWORD) * 3;
            }

            const size_t paletteOffset = colorTableOffset + masksSize;
            const size_t paletteSize   = colorTableEntries * sizeof(RGBQUAD);

            if (paletteOffset + paletteSize > dibSize)
                return std::nullopt;

            const RGBQUAD* palette     = nullptr;
            if (colorTableEntries > 0)
            {
                palette                = reinterpret_cast<const RGBQUAD*>(bytes + paletteOffset);
            }

            size_t pixelOffset         = paletteOffset + paletteSize;
            if (header->biSize > sizeof(BITMAPINFOHEADER))
            {
                pixelOffset            = header->biSize;

                if (header->biSize == sizeof(BITMAPINFOHEADER) && masksSize != 0)
                {
                    pixelOffset       += masksSize;
                }

                pixelOffset           += paletteSize;
            }

            if (header->biSize == sizeof(BITMAPINFOHEADER) && masksSize != 0)
            {
                pixelOffset            = sizeof(BITMAPINFOHEADER) + masksSize + paletteSize;
            }

            if (pixelOffset >= dibSize)
                return std::nullopt;

            const size_t requiredPixelBytes = stride * static_cast<size_t>(height);

            if (pixelOffset + requiredPixelBytes > dibSize)
                return std::nullopt;

            const uint8_t* sourcePixels    = bytes + pixelOffset;

            ClipboardImage image;
            image.Width  = static_cast<uint32_t>(width);
            image.Height = static_cast<uint32_t>(height);

            image.Pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);

            uint32_t redMask   = 0x00FF0000;
            uint32_t greenMask = 0x0000FF00;
            uint32_t blueMask  = 0x000000FF;
            uint32_t alphaMask = 0xFF000000;

            if (header->biCompression == BI_BITFIELDS && bpp == 32)
            {
                if (header->biSize == sizeof(BITMAPINFOHEADER))
                {
                    const uint32_t* masks = reinterpret_cast<const uint32_t*>(bytes + sizeof(BITMAPINFOHEADER));

                    redMask   = masks[0];
                    greenMask = masks[1];
                    blueMask  = masks[2];
                    alphaMask = 0;
                }
                else if (header->biSize >= sizeof(BITMAPV4HEADER))
                {
                    const BITMAPV4HEADER* v4 = reinterpret_cast<const BITMAPV4HEADER*>(bytes);

                    redMask   = v4->bV4RedMask;
                    greenMask = v4->bV4GreenMask;
                    blueMask  = v4->bV4BlueMask;

                    if (header->biSize >= sizeof(BITMAPV5HEADER))
                    {
                        const BITMAPV5HEADER* v5 = reinterpret_cast<const BITMAPV5HEADER*>(bytes);
                        alphaMask                = v5->bV5AlphaMask;
                    }
                }
            }

            auto ExtractChannel = [](uint32_t value, uint32_t mask) -> uint8_t
            {
                if (mask == 0)
                    return 255;

                unsigned int shift = 0;

                while (((mask >> shift) & 1u) == 0u)
                    ++shift;

                uint32_t channel   = (value & mask) >> shift;
                uint32_t maxValue  = mask >> shift;

                if (maxValue == 0)
                    return 0;

                return static_cast<uint8_t>((channel * 255u + maxValue / 2u) / maxValue);
            };

            for (int y = 0; y < height; y++)
            {
                const int sourceY  = bottomUp ? height - 1 - y : y;
                const uint8_t* src = sourcePixels + static_cast<size_t>(sourceY) * stride;
                uint8_t* dst       = image.Pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(width) * 4;

                for (int x = 0; x < width; x++)
                {
                    uint8_t r = 0;
                    uint8_t g = 0;
                    uint8_t b = 0;
                    uint8_t a = 255;

                    switch (bpp)
                    {
                        case 32:
                        {
                            const uint32_t pixel = reinterpret_cast<const uint32_t*>(src)[x];

                            if (header->biCompression == BI_BITFIELDS)
                            {
                                r = ExtractChannel(pixel, redMask);
                                g = ExtractChannel(pixel, greenMask);
                                b = ExtractChannel(pixel, blueMask);
                                a = alphaMask != 0 ? ExtractChannel(pixel, alphaMask) : 255;
                            }
                            else
                            {
                                const uint8_t* pixelBytes = src + x * 4;

                                b = pixelBytes[0];
                                g = pixelBytes[1];
                                r = pixelBytes[2];
                                a = pixelBytes[3] == 0 ? 255 : pixelBytes[3];
                            }

                            break;
                        }

                        case 24:
                        {
                            const uint8_t* pixel = src + x * 3;

                            b = pixel[0];
                            g = pixel[1];
                            r = pixel[2];

                            break;
                        }

                        case 8:
                        {
                            const uint8_t index = src[x];

                            if (!palette || index >= colorTableEntries)
                            {
                                return std::nullopt;
                            }

                            const RGBQUAD& color = palette[index];

                            r = color.rgbRed;
                            g = color.rgbGreen;
                            b = color.rgbBlue;

                            break;
                        }

                        case 4:
                        {
                            const uint8_t packed = src[x / 2];
                            const uint8_t index = (x & 1) ? (packed & 0x0F) : (packed >> 4);

                            if (!palette || index >= colorTableEntries)
                            {
                                return std::nullopt;
                            }

                            const RGBQUAD& color = palette[index];

                            r = color.rgbRed;
                            g = color.rgbGreen;
                            b = color.rgbBlue;

                            break;
                        }

                        case 1:
                        {
                            const uint8_t packed = src[x / 8];
                            const uint8_t index  = (packed >> (7 - (x % 8))) & 1;

                            if (!palette || index >= colorTableEntries)
                            {
                                return std::nullopt;
                            }

                            const RGBQUAD& color = palette[index];

                            r = color.rgbRed;
                            g = color.rgbGreen;
                            b = color.rgbBlue;

                            break;
                        }
                    }

                    dst[x * 4 + 0] = r;
                    dst[x * 4 + 1] = g;
                    dst[x * 4 + 2] = b;
                    dst[x * 4 + 3] = a;
                }
            }

            return image;
        }

        std::optional<ClipboardImage> ConvertHBitmap(HBITMAP bitmap)
        {
            if (!bitmap)
                return std::nullopt;

            BITMAP bitmapInfo{};

            if (GetObject(bitmap, sizeof(BITMAP), &bitmapInfo) == 0)
            {
                return std::nullopt;
            }

            if (bitmapInfo.bmWidth <= 0 || bitmapInfo.bmHeight <= 0)
            {
                return std::nullopt;
            }

            BITMAPINFO info{};

            info.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth       = bitmapInfo.bmWidth;
            info.bmiHeader.biHeight      = -bitmapInfo.bmHeight;
            info.bmiHeader.biPlanes      = 1;
            info.bmiHeader.biBitCount    = 32;
            info.bmiHeader.biCompression = BI_RGB;

            ClipboardImage result;
            result.Width  = static_cast<uint32_t>(bitmapInfo.bmWidth);
            result.Height = static_cast<uint32_t>(bitmapInfo.bmHeight);

            result.Pixels.resize(static_cast<size_t>(result.Width) * static_cast<size_t>(result.Height) * 4);

            HDC dc = GetDC(nullptr);

            if (!dc)
                return std::nullopt;

            const int resultCode = GetDIBits(dc, bitmap, 0, result.Height, result.Pixels.data(), &info, DIB_RGB_COLORS);
            ReleaseDC(nullptr, dc);

            if (resultCode == 0)
                return std::nullopt;

            for (size_t i = 0; i < result.Pixels.size(); i += 4)
            {
                std::swap(result.Pixels[i + 0], result.Pixels[i + 2]);
                result.Pixels[i + 3] = 255;
            }

            return result;
        }
    }

    std::optional<ClipboardImage> Clipboard::GetImage()
    {
        if (!OpenClipboard(nullptr))
            return std::nullopt;

        if (IsClipboardFormatAvailable(CF_DIBV5))
        {
            HANDLE handle = GetClipboardData(CF_DIBV5);
            if (handle)
            {
                const SIZE_T size = GlobalSize(handle);
                void* data        = GlobalLock(handle);

                if (data)
                {
                    auto image    = ConvertDIB(data, static_cast<size_t>(size));
                    GlobalUnlock(handle);

                    if (image)
                    {
                        CloseClipboard();
                        return image;
                    }
                }
            }
        }

        if (IsClipboardFormatAvailable(CF_DIB))
        {
            HANDLE handle          = GetClipboardData(CF_DIB);
            if (handle)
            {
                const SIZE_T size = GlobalSize(handle);
                void* data        = GlobalLock(handle);

                if (data)
                {
                    auto image    = ConvertDIB(data, static_cast<size_t>(size));
                    GlobalUnlock(handle);

                    if (image)
                    {
                        CloseClipboard();
                        return image;
                    }
                }
            }
        }

        if (IsClipboardFormatAvailable(CF_BITMAP))
        {
            HBITMAP bitmap = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
            if (bitmap)
            {
                auto image = ConvertHBitmap(bitmap);

                if (image)
                {
                    CloseClipboard();
                    return image;
                }
            }
        }

        CloseClipboard();

        return std::nullopt;
    }

} // namespace QB

#else
namespace QB
{
    std::optional<ClipboardImage> Clipboard::GetImage()
    {
        return std::nullopt;
    }

} // namespace QB
#endif