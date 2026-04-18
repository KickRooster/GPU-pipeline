#include "TextureLoader.h"
#include <fstream>

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif

#include "../../thirdpatry/stb/stb_image.h"
#include "../misc/FileTool.h"

using namespace std;

DXGI_FORMAT TextureLoader::GetDXGIFormat(int Channels, bool IsHDR, bool Is16Bit, bool IsSRGB)
{
    if (IsHDR)
    {
        switch (Channels)
        {
        case 1:
            return DXGI_FORMAT_R32_FLOAT;
        case 2:
            return DXGI_FORMAT_R32G32_FLOAT;
        case 3:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case 4:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        default:
            return DXGI_FORMAT_UNKNOWN;
        }
    }
    else if (Is16Bit)
    {
        switch (Channels)
        {
        case 1:
            return DXGI_FORMAT_R16_UNORM;
        case 2:
            return DXGI_FORMAT_R16G16_UNORM;
        case 4:
            return DXGI_FORMAT_R16G16B16A16_UNORM;
        default:
            return DXGI_FORMAT_UNKNOWN;
        }
    }
    else
    {
        switch (Channels)
        {
        case 1:
            return DXGI_FORMAT_R8_UNORM;
        case 2:
            return DXGI_FORMAT_R8G8_UNORM;
        // 3-channel textures are converted to 4-channel
        case 3:
        case 4:
            if (IsSRGB)
            {
                return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            }
            else
            {
                return DXGI_FORMAT_R8G8B8A8_UNORM;
            }
        default:
            return DXGI_FORMAT_UNKNOWN;
        }
    }
}

ErrorCode TextureLoader::LoadTexture(const std::string& FilePath, bool IsSRGB, Texture& OutTextureInstance)
{
    const string TextureFullPath = FileTool::GetInstance().GetTextureFullPath(FilePath);
    
    std::ifstream File(TextureFullPath);
    if (!File.good())
    {
        return ErrorCode::TextureNotExist;
    }
    File.close();
    
    OutTextureInstance.IsHDR = stbi_is_hdr(TextureFullPath.c_str());
    OutTextureInstance.Is16Bit = stbi_is_16_bit(TextureFullPath.c_str());

    int LoadWidth = 0;
    int LoadHeight = 0;

    if (OutTextureInstance.IsHDR)
    {
        float* Data = stbi_loadf(TextureFullPath.c_str(), &LoadWidth, &LoadHeight, &OutTextureInstance.Channels, 0);
        if (Data == nullptr)
        {
            return ErrorCode::TextureLoadFailed;
        }

        MipLevel BaseMip;
        BaseMip.Width = LoadWidth;
        BaseMip.Height = LoadHeight;
        size_t ByteSize = LoadWidth * LoadHeight * OutTextureInstance.Channels * sizeof(float);
        BaseMip.Data.resize(ByteSize);
        memcpy(BaseMip.Data.data(), Data, ByteSize);
        stbi_image_free(Data);

        OutTextureInstance.Mips.push_back(std::move(BaseMip));
    }
    else if (OutTextureInstance.Is16Bit)
    {
        stbi_us* Data = stbi_load_16(TextureFullPath.c_str(), &LoadWidth, &LoadHeight, &OutTextureInstance.Channels, 0);
        if (Data == nullptr)
        {
            return ErrorCode::TextureLoadFailed;
        }

        OutTextureInstance.OriginalChannels = OutTextureInstance.Channels;

        MipLevel BaseMip;
        BaseMip.Width = LoadWidth;
        BaseMip.Height = LoadHeight;

        if (OutTextureInstance.Channels == 3)
        {
            const int PixelCount = LoadWidth * LoadHeight;
            BaseMip.Data.resize(PixelCount * 4 * sizeof(uint16_t));
            uint16_t* Dst = reinterpret_cast<uint16_t*>(BaseMip.Data.data());
            for (int i = 0; i < PixelCount; ++i)
            {
                Dst[i * 4 + 0] = Data[i * 3 + 0];
                Dst[i * 4 + 1] = Data[i * 3 + 1];
                Dst[i * 4 + 2] = Data[i * 3 + 2];
                Dst[i * 4 + 3] = 65535;
            }
            stbi_image_free(Data);
            OutTextureInstance.Channels = 4;
        }
        else
        {
            size_t ByteSize = LoadWidth * LoadHeight * OutTextureInstance.Channels * sizeof(uint16_t);
            BaseMip.Data.resize(ByteSize);
            memcpy(BaseMip.Data.data(), Data, ByteSize);
            stbi_image_free(Data);
        }

        OutTextureInstance.Mips.push_back(std::move(BaseMip));
    }
    else
    {
        stbi_uc* Data = stbi_load(TextureFullPath.c_str(), &LoadWidth, &LoadHeight, &OutTextureInstance.Channels, 0);
        if (Data == nullptr)
        {
            return ErrorCode::TextureLoadFailed;
        }

        OutTextureInstance.OriginalChannels = OutTextureInstance.Channels;
        
        MipLevel BaseMip;
        BaseMip.Width = LoadWidth;
        BaseMip.Height = LoadHeight;

        if (OutTextureInstance.Channels == 3)
        {
            const int PixelCount = LoadWidth * LoadHeight;
            BaseMip.Data.resize(PixelCount * 4);
            for (int i = 0; i < PixelCount; ++i)
            {
                BaseMip.Data[i * 4 + 0] = Data[i * 3 + 0];
                BaseMip.Data[i * 4 + 1] = Data[i * 3 + 1];
                BaseMip.Data[i * 4 + 2] = Data[i * 3 + 2];
                BaseMip.Data[i * 4 + 3] = 255;
            }
            stbi_image_free(Data);
            OutTextureInstance.Channels = 4;
        }
        else
        {
            size_t ByteSize = LoadWidth * LoadHeight * OutTextureInstance.Channels;
            BaseMip.Data.resize(ByteSize);
            memcpy(BaseMip.Data.data(), Data, ByteSize);
            stbi_image_free(Data);
        }

        OutTextureInstance.Mips.push_back(std::move(BaseMip));
    }
    
    OutTextureInstance.IsSRGB = IsSRGB;
    OutTextureInstance.Format = GetDXGIFormat(OutTextureInstance.Channels, OutTextureInstance.IsHDR, OutTextureInstance.Is16Bit, IsSRGB);

    // Generate mipmap chain (box filter downsampling)
    OutTextureInstance.GenerateMipmaps();

    return ErrorCode::OK;
}