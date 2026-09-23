#pragma once

// std
#include <memory>
#include <vector>


namespace QB
{
    enum class ImageFormat : uint8_t
    {
        NONE = 0,

        R8,
        R8_INT,
        R8_UINT,
        R32_INT,
        R32_UINT,
        R32_FLOAT,
        R16_FLOAT,

        RG8,
        RG32_UINT,
        RG16_FLOAT,

        RGB8,
        RGB32_FLOAT,

        RGBA8,
        RGBA16_FLOAT,
        RGBA32_FLOAT
    };

    enum class ImageWrap : uint8_t
    {
        NONE = 0,
        REPEAT,
        MIRRORED_REPEAT,
        CLAMP_TO_EDGE,
        CLAMP_TO_BORDER
    };

    enum class ImageFilter : uint8_t
    {
        NONE = 0,
        LINEAR,
        NEAREST
    };

    struct ImageSpecification
    {
        uint32_t Width          = 1;
        uint32_t Height         = 1;
        ImageFormat Format      = ImageFormat::RGBA8;
        ImageFilter MinFilter   = ImageFilter::LINEAR;
        ImageFilter MagFilter   = ImageFilter::LINEAR;
        ImageWrap WrapU         = ImageWrap::REPEAT;
        ImageWrap WrapV         = ImageWrap::REPEAT;
        ImageWrap WrapW         = ImageWrap::REPEAT;
    };

    class Image
    {
        public:
            Image(const ImageSpecification& p_Spec);
            Image(const ImageSpecification& p_Spec, const uint8_t* p_Data, size_t p_Size);
            ~Image();

            void SetData(const void* p_Data, size_t p_Size);
            std::vector<uint8_t> GetData() const;

            uint32_t GetWidth() const { return m_Width; }
            uint32_t GetHeight() const { return m_Height; }
            uint32_t GetChannels() const { return m_Channels; }
            uint32_t GetBytesPerChannels() const { return m_BytesPerChannels; }
            const ImageSpecification& GetSpecification() const { return m_Spec; }
            uint64_t GetEstimatedSize() const { return (uint64_t)m_Width * m_Height * m_Channels * m_BytesPerChannels; }
            uint32_t GetID() const { return m_RendererID; }

            bool operator== (const Image& p_Other) const
            {
                return m_RendererID == p_Other.m_RendererID;
            }
            
            static std::shared_ptr<Image> Create(const ImageSpecification& p_Spec, const uint8_t* p_Data, size_t p_Size);
            static std::shared_ptr<Image> Create(const std::vector<uint8_t>& p_Data, uint32_t p_Width, uint32_t p_Height);

        private:
            void Init(const ImageSpecification& p_Spec);

        private:
            ImageSpecification m_Spec = {};

            uint32_t m_Width            = 0;
            uint32_t m_Height           = 0;
            uint32_t m_Channels         = 0;
            uint32_t m_BytesPerChannels = 0;
            uint32_t m_RendererID       = 0;
            uint32_t m_Format           = 0;
            uint32_t m_FormatType       = 0;
            uint32_t m_InternalFormat   = 0;
    };

} // namespace QB
