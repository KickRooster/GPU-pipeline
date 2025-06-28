#include "TextureLoader.h"
#include <fstream>

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif

#include "../../thirdpatry/stb/stb_image.h"
#include "../asset/FileTool.h"

using namespace std;

DXGI_FORMAT TextureLoader::GetDXGIFormat(int Channels, bool IsHDR)
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
    else
    {
        switch (Channels)
        {
        case 1:
            return DXGI_FORMAT_R8_UNORM;
        case 2:
            return DXGI_FORMAT_R8G8_UNORM;
        case 3:
            return DXGI_FORMAT_R8G8B8A8_UNORM; // 3-channel textures are converted to 4-channel
        case 4:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        default:
            return DXGI_FORMAT_UNKNOWN;
        }
    }
}

ErrorCode TextureLoader::LoadTexture(const std::string& FilePath, Texture& OutTextureInstance)
{
    string TextureFullPath = FileTool::GetInstance().GetTextureFullPath(FilePath);
    
    std::ifstream File(TextureFullPath);
    if (!File.good())
    {
        return ErrorCode::TextureNotExist;
    }
    File.close();
    
    OutTextureInstance.IsHDR = stbi_is_hdr(TextureFullPath.c_str());
    
    if (OutTextureInstance.IsHDR)
    {
        float* Data = stbi_loadf(TextureFullPath.c_str(), &OutTextureInstance.Width, &OutTextureInstance.Height, &OutTextureInstance.Channels, 0);
        if (Data == nullptr)
        {
            return ErrorCode::TextureLoadFailed;
        }
        OutTextureInstance.Data = Data;
        OutTextureInstance.ByteSize = OutTextureInstance.Width * OutTextureInstance.Height * OutTextureInstance.Channels * sizeof(float);
    }
    else
    {
        stbi_uc* Data = stbi_load(TextureFullPath.c_str(), &OutTextureInstance.Width, &OutTextureInstance.Height, &OutTextureInstance.Channels, 0);
        if (Data == nullptr)
        {
            return ErrorCode::TextureLoadFailed;
        }

        OutTextureInstance.OriginalChannels = OutTextureInstance.Channels;
        
        if (OutTextureInstance.Channels == 3)
        {
            const int PixelCount = OutTextureInstance.Width * OutTextureInstance.Height;
            stbi_uc* RGBAData = (stbi_uc*)malloc(PixelCount * 4);
            if (RGBAData)
            {
                for (int i = 0; i < PixelCount; ++i)
                {
                    RGBAData[i * 4 + 0] = Data[i * 3 + 0]; // R
                    RGBAData[i * 4 + 1] = Data[i * 3 + 1]; // G
                    RGBAData[i * 4 + 2] = Data[i * 3 + 2]; // B
                    RGBAData[i * 4 + 3] = 255;             // A
                }
                stbi_image_free(Data);
                OutTextureInstance.Data = RGBAData;
                OutTextureInstance.Channels = 4;
                OutTextureInstance.ByteSize = PixelCount * 4;
            }
            else
            {
                stbi_image_free(Data);
                
                return ErrorCode::AllocateTextureMemoryFailed;
            }
        }
        else
        {
            OutTextureInstance.Data = Data;
            OutTextureInstance.ByteSize = OutTextureInstance.Width * OutTextureInstance.Height * OutTextureInstance.Channels;
        }
    }
    
    OutTextureInstance.Format = GetDXGIFormat(OutTextureInstance.Channels, OutTextureInstance.IsHDR);
    
    return ErrorCode::OK;
}