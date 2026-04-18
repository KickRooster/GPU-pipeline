#pragma once
#include "../misc/Base.h"
#include "../misc/DesignPatterns.h"
#include "Texture.h"

class TextureLoader : public Singleton<TextureLoader>
{
private:
    DXGI_FORMAT GetDXGIFormat(int Channels, bool IsHDR, bool Is16Bit, bool IsSRGB);
    
public:
    ErrorCode LoadTexture(const std::string& FilePath, bool IsSRGB, Texture& OutTextureInstance);
};