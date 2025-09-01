#include "Texture.h"
#include "../../thirdpatry/stb/stb_image.h"

using namespace DirectX;

Texture::Texture(Texture&& InTexture) noexcept
{
    Data = InTexture.Data;
    Width = InTexture.Width;
    Height = InTexture.Height;
    OriginalChannels = InTexture.OriginalChannels;
    Channels = InTexture.Channels;
    IsHDR = InTexture.IsHDR;
    IsSRGB = InTexture.IsSRGB;
    Format = InTexture.Format;
    ByteSize = InTexture.ByteSize;

    InTexture.Data = nullptr;
    InTexture.Width = 0;
    InTexture.Height = 0;
    InTexture.ByteSize = 0;
}

XMFLOAT4 Texture::Sample(const XMFLOAT2& UV) const
{
    float PixelU = UV.x * static_cast<float>(Width - 1);
    float PixelV = UV.y * static_cast<float>(Height - 1);
    
    int PixelX = static_cast<int>(PixelU) % Width;
    int PixelY = static_cast<int>(PixelV);
    
    const float* PixelData = static_cast<const float*>(Data);
    int PixelIndex = (PixelY * Width + PixelX) * Channels;
    
    XMFLOAT4 Color;
    Color.x = PixelData[PixelIndex];
    Color.y = PixelData[PixelIndex + 1];
    Color.z = PixelData[PixelIndex + 2];
    //  Assign a default value to alpha.
    Color.w = 1.0f;
    
    if (Channels == 4)
    {
        Color.w = PixelData[PixelIndex + 3];
    }
    
    return Color;
}

Texture::~Texture()
{
    if (Data != nullptr)
    {
        if (IsHDR)
        {
            stbi_image_free(Data);
        }
        else
        {
            if (OriginalChannels == 3)
            {
                free(Data);
            }
            else
            {
                stbi_image_free(Data);
            }
        }
    }
}