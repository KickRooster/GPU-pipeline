#include "Texture.h"
#include "../../thirdpatry/stb/stb_image.h"

using namespace DirectX;

Texture::Texture(Texture&& InTexture) noexcept
{
    OriginalChannels = InTexture.OriginalChannels;
    Channels = InTexture.Channels;
    IsHDR = InTexture.IsHDR;
    Is16Bit = InTexture.Is16Bit;
    IsSRGB = InTexture.IsSRGB;
    Format = InTexture.Format;
    Mips = std::move(InTexture.Mips);
}

XMFLOAT4 Texture::Sample(const XMFLOAT2& UV) const
{
    int W = GetWidth();
    int H = GetHeight();

    float PixelU = UV.x * static_cast<float>(W - 1);
    float PixelV = UV.y * static_cast<float>(H - 1);

    int PixelX = static_cast<int>(PixelU) % W;
    int PixelY = static_cast<int>(PixelV) % H;

    const float* PixelData = reinterpret_cast<const float*>(Mips[0].Data.data());
    int PixelIndex = (PixelY * W + PixelX) * Channels;

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

void Texture::GenerateMipmaps()
{
    if (Mips.empty() || (GetWidth() <= 1 && GetHeight() <= 1))
    {
        return;
    }

    int BytesPerPixel = Channels * (IsHDR ? 4 : (Is16Bit ? 2 : 1));
    int MipWidth = GetWidth();
    int MipHeight = GetHeight();

    while (MipWidth > 1 || MipHeight > 1)
    {
        int NewWidth = (MipWidth > 1) ? MipWidth / 2 : 1;
        int NewHeight = (MipHeight > 1) ? MipHeight / 2 : 1;

        MipLevel Mip;
        Mip.Width = NewWidth;
        Mip.Height = NewHeight;
        Mip.Data.resize(NewWidth * NewHeight * BytesPerPixel);

        // Use float coordinate mapping to handle non-power-of-two sizes correctly
        float ScaleX = static_cast<float>(MipWidth) / static_cast<float>(NewWidth);
        float ScaleY = static_cast<float>(MipHeight) / static_cast<float>(NewHeight);

        const unsigned char* PrevData = Mips.back().Data.data();

        if (IsHDR)
        {
            const float* Src = reinterpret_cast<const float*>(PrevData);
            float* Dst = reinterpret_cast<float*>(Mip.Data.data());
            for (int Y = 0; Y < NewHeight; Y++)
            {
                for (int X = 0; X < NewWidth; X++)
                {
                    int SX = static_cast<int>(X * ScaleX);
                    int SY = static_cast<int>(Y * ScaleY);
                    int SX1 = (SX + 1 < MipWidth) ? SX + 1 : SX;
                    int SY1 = (SY + 1 < MipHeight) ? SY + 1 : SY;

                    for (int C = 0; C < Channels; C++)
                    {
                        float V00 = Src[(SY  * MipWidth + SX)  * Channels + C];
                        float V10 = Src[(SY  * MipWidth + SX1) * Channels + C];
                        float V01 = Src[(SY1 * MipWidth + SX)  * Channels + C];
                        float V11 = Src[(SY1 * MipWidth + SX1) * Channels + C];
                        Dst[(Y * NewWidth + X) * Channels + C] = (V00 + V10 + V01 + V11) * 0.25f;
                    }
                }
            }
        }
        else if (Is16Bit)
        {
            const uint16_t* Src = reinterpret_cast<const uint16_t*>(PrevData);
            uint16_t* Dst = reinterpret_cast<uint16_t*>(Mip.Data.data());
            for (int Y = 0; Y < NewHeight; Y++)
            {
                for (int X = 0; X < NewWidth; X++)
                {
                    int SX = static_cast<int>(X * ScaleX);
                    int SY = static_cast<int>(Y * ScaleY);
                    int SX1 = (SX + 1 < MipWidth) ? SX + 1 : SX;
                    int SY1 = (SY + 1 < MipHeight) ? SY + 1 : SY;

                    for (int C = 0; C < Channels; C++)
                    {
                        int V00 = Src[(SY  * MipWidth + SX)  * Channels + C];
                        int V10 = Src[(SY  * MipWidth + SX1) * Channels + C];
                        int V01 = Src[(SY1 * MipWidth + SX)  * Channels + C];
                        int V11 = Src[(SY1 * MipWidth + SX1) * Channels + C];
                        Dst[(Y * NewWidth + X) * Channels + C] = static_cast<uint16_t>((V00 + V10 + V01 + V11 + 2) / 4);
                    }
                }
            }
        }
        else
        {
            const unsigned char* Src = PrevData;
            unsigned char* Dst = Mip.Data.data();
            for (int Y = 0; Y < NewHeight; Y++)
            {
                for (int X = 0; X < NewWidth; X++)
                {
                    int SX = static_cast<int>(X * ScaleX);
                    int SY = static_cast<int>(Y * ScaleY);
                    int SX1 = (SX + 1 < MipWidth) ? SX + 1 : SX;
                    int SY1 = (SY + 1 < MipHeight) ? SY + 1 : SY;

                    for (int C = 0; C < Channels; C++)
                    {
                        int V00 = Src[(SY  * MipWidth + SX)  * Channels + C];
                        int V10 = Src[(SY  * MipWidth + SX1) * Channels + C];
                        int V01 = Src[(SY1 * MipWidth + SX)  * Channels + C];
                        int V11 = Src[(SY1 * MipWidth + SX1) * Channels + C];

                        // sRGB color channels (not alpha) need linear-space filtering
                        if (IsSRGB && C < 3)
                        {
                            float L00 = powf(V00 / 255.0f, 2.2f);
                            float L10 = powf(V10 / 255.0f, 2.2f);
                            float L01 = powf(V01 / 255.0f, 2.2f);
                            float L11 = powf(V11 / 255.0f, 2.2f);
                            float Avg = (L00 + L10 + L01 + L11) * 0.25f;
                            Dst[(Y * NewWidth + X) * Channels + C] = static_cast<unsigned char>(powf(Avg, 1.0f / 2.2f) * 255.0f + 0.5f);
                        }
                        else
                        {
                            Dst[(Y * NewWidth + X) * Channels + C] = static_cast<unsigned char>((V00 + V10 + V01 + V11 + 2) / 4);
                        }
                    }
                }
            }
        }

        Mips.push_back(std::move(Mip));

        MipWidth = NewWidth;
        MipHeight = NewHeight;
    }
}

Texture::~Texture()
{
}