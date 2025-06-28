#include "Texture.h"
#include "../../thirdpatry/stb/stb_image.h"

Texture::Texture(Texture&& InTexture)
{
    Data = InTexture.Data;
    Width = InTexture.Width;
    Height = InTexture.Height;
    OriginalChannels = InTexture.OriginalChannels;
    Channels = InTexture.Channels;
    IsHDR = InTexture.IsHDR;
    Format = InTexture.Format;
    ByteSize = InTexture.ByteSize;

    InTexture.Data = nullptr;
    InTexture.Width = 0;
    InTexture.Height = 0;
    InTexture.ByteSize = 0;
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