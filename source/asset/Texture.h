#pragma once
#include "../misc/Math.h"
#include <vector>
#include <string>
#include <dxgi1_6.h>

struct PBRTextureNamesPatch
{
    std::string AlbedoPath;
    std::string NormalPath;
    std::string MetallicPath;
    std::string RoughnessPath;
    std::vector<std::string> OtherTexturePaths;
};

struct MipLevel
{
    std::vector<unsigned char> Data;
    int Width = 0;
    int Height = 0;
};

struct Texture
{
    int OriginalChannels = 0;
    int Channels = 0;
    bool IsHDR = false;
    bool IsSRGB = false;
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;

    // Mips[0] = base level (mip 0), Mips[1] = mip 1, etc.
    std::vector<MipLevel> Mips;

    int GetWidth() const { return Mips.empty() ? 0 : Mips[0].Width; }
    int GetHeight() const { return Mips.empty() ? 0 : Mips[0].Height; }
    int GetMipCount() const { return static_cast<int>(Mips.size()); }
    const void* GetData() const { return Mips.empty() ? nullptr : Mips[0].Data.data(); }

    Texture() = default;
    Texture(Texture&& InTexture) noexcept;
    DirectX::XMFLOAT4 Sample(const DirectX::XMFLOAT2& UV) const;
    void GenerateMipmaps();
    ~Texture();
};