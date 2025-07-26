#pragma once
#include "../misc/Base.h"
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

struct Texture
{
    void* Data = nullptr;
    int Width = 0;
    int Height = 0;
    int OriginalChannels = 0;
    int Channels = 0;
    bool IsHDR = false;
    bool IsSRGB = false;
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
    size_t ByteSize = 0;

    Texture() = default;
    Texture(Texture&& InTexture);
    ~Texture();
};