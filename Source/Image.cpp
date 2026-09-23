#include "Image.h"

// std
#include <cassert>
#include <iostream>

// lib
#include <glad/glad.h>



namespace QB
{
    namespace
    {
        static uint32_t TextureFormatToGLFormat(ImageFormat p_Format)
        {
            switch (p_Format)
            {
                case ImageFormat::R8:
                    return GL_RED;
                case ImageFormat::R32_INT:
                case ImageFormat::R32_UINT:
                    return GL_RED_INTEGER;
                case ImageFormat::RG32_UINT:
                    return GL_RG;
                case ImageFormat::RGB32_FLOAT:
                case ImageFormat::RGB8:
                    return GL_RGB;
                case ImageFormat::RGBA32_FLOAT:
                case ImageFormat::RGBA8:
                    return GL_RGBA;
            }

            std::cerr << "Unsupported texture format!\n";
            return 0;
        }

        static uint32_t TextureFormatToGLInternalFormat(ImageFormat p_Format)
        {
            switch (p_Format)
            {
                case ImageFormat::R8:           return GL_R8;
                case ImageFormat::R32_INT:      return GL_R32I;
                case ImageFormat::R32_UINT:     return GL_R32UI;
                case ImageFormat::RG32_UINT:    return GL_RG32UI;
                case ImageFormat::RGBA32_FLOAT: return GL_RGBA32F;
                case ImageFormat::RGB8:         return GL_RGB8;
                case ImageFormat::RGB32_FLOAT:  return GL_RGB32F;
                case ImageFormat::RGBA8:        return GL_RGBA8;
            }

            std::cerr << "Unsupported texture format!\n";
            return 0;
        }

        static uint32_t TextureFormatToGLType(ImageFormat p_Format)
        {
            switch (p_Format)
            {
                case ImageFormat::R32_INT:
                    return GL_INT;
                case ImageFormat::RG32_UINT:
                case ImageFormat::R32_UINT:
                    return GL_UNSIGNED_INT;
                case ImageFormat::RGBA32_FLOAT:
                case ImageFormat::RGB32_FLOAT:
                    return GL_FLOAT;
                case ImageFormat::R8:
                case ImageFormat::RGB8:
                case ImageFormat::RGBA8:
                    return GL_UNSIGNED_BYTE;
                    return GL_UNSIGNED_BYTE | GL_UNSIGNED_INT;
            }

            std::cerr << "Unsupported texture format!\n";
            return 0;
        }

        static uint8_t TextureFormatToChannels(ImageFormat p_Format)
        {
            switch (p_Format)
            {
                case ImageFormat::R8:
                case ImageFormat::R32_INT:
                case ImageFormat::R32_UINT:
                    return 1;

                case ImageFormat::RG32_UINT:
                    return 2;

                case ImageFormat::RGB8:
                case ImageFormat::RGB32_FLOAT:
                    return 3;

                case ImageFormat::RGBA8:
                case ImageFormat::RGBA32_FLOAT:
                    return 4;
            }

            std::cerr << "Unknown texture format!\n";
            return 0;
        }

        static uint8_t TextureFormatToBytesPerChannels(ImageFormat p_Format)
        {
            switch (p_Format)
            {
                case ImageFormat::R8:
                case ImageFormat::RGB8:
                case ImageFormat::RGBA8:
                    return 1;

                case ImageFormat::R32_INT:
                case ImageFormat::R32_UINT:
                case ImageFormat::RG32_UINT:
                case ImageFormat::RGBA32_FLOAT:
                case ImageFormat::RGB32_FLOAT:
                    return 4;
            }

            std::cerr << "Unknown texture format!\n";
            return 0;
        }

        static uint32_t TextureFilterToGL(ImageFilter p_Filter, bool p_Mipmap)
        {
            switch (p_Filter)
            {
                case ImageFilter::LINEAR: return p_Mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
                case ImageFilter::NEAREST: return p_Mipmap ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST;
            }

            std::cerr << "Unsupported texture filter!\n";
            return 0;
        }

        static uint32_t TextureWrapToGL(ImageWrap p_Wrap)
        {
            switch (p_Wrap)
            {
                case ImageWrap::REPEAT:           return GL_REPEAT;
                case ImageWrap::MIRRORED_REPEAT:  return GL_MIRRORED_REPEAT;
                case ImageWrap::CLAMP_TO_BORDER:  return GL_CLAMP_TO_BORDER;
                case ImageWrap::CLAMP_TO_EDGE:    return GL_CLAMP_TO_EDGE;
            }

            std::cerr << "Unsupported texture wrap!\n";
            return 0;
        }

    } // namespace

    void Image::Init(const ImageSpecification& p_Spec)
    {
        m_Spec             = p_Spec;
        m_Format           = TextureFormatToGLFormat(p_Spec.Format);
        m_InternalFormat   = TextureFormatToGLInternalFormat(p_Spec.Format);
        m_FormatType       = TextureFormatToGLType(m_Spec.Format);
        m_Width            = p_Spec.Width;
        m_Height           = p_Spec.Height;
        m_Channels         = TextureFormatToChannels(p_Spec.Format);
        m_BytesPerChannels = TextureFormatToBytesPerChannels(p_Spec.Format);

        glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
        glTextureStorage2D(m_RendererID, 1, m_InternalFormat, m_Width, m_Height);

        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, TextureWrapToGL(p_Spec.WrapU));
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, TextureWrapToGL(p_Spec.WrapV));
        glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, TextureFilterToGL(p_Spec.MinFilter, false));
        glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, TextureFilterToGL(p_Spec.MagFilter, false));
    }

    void Image::SetData(const void* p_Data, size_t p_Size)
    {
        assert(p_Size >= size_t(m_Width * m_Height * m_Channels * m_BytesPerChannels) && "Data must be entire texture!");

        glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_Format, m_FormatType, p_Data);
    }

    Image::Image(const ImageSpecification& p_Spec)
    {
        Init(p_Spec);
    }

    Image::Image(const ImageSpecification& p_Spec, const uint8_t* p_Data, size_t p_Size)
    {
        Init(p_Spec);
        SetData(p_Data, p_Size);
    }

    Image::~Image()
    {
        glDeleteTextures(1, &m_RendererID);
    }

    std::vector<uint8_t> Image::GetData() const
    {
        std::vector<uint8_t> buffer(GetEstimatedSize());
        glGetTextureImage(m_RendererID, 0, m_Format, m_FormatType, (GLsizei)buffer.size(), buffer.data());

        return buffer;
    }

    std::shared_ptr<Image> Image::Create(const ImageSpecification& p_Spec, const uint8_t* p_Data, size_t p_Size)
    {
        return std::make_shared<Image>(p_Spec, p_Data, p_Size);
    }

    std::shared_ptr<Image> Image::Create(const std::vector<uint8_t>& p_Data, uint32_t p_Width, uint32_t p_Height)
    {
        ImageSpecification spec = {};
        spec.Format             = ImageFormat::RGBA8;
        spec.Width              = p_Width;
        spec.Height             = p_Height;

        return Create(spec, p_Data.data(), p_Data.size());
    }

} // namespace QB
