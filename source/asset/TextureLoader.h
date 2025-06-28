#pragma once
#include "../misc/Base.h"
#include "../misc/DesignPatterns.h"
#include "Texture.h"

class TextureLoader : public Singleton<TextureLoader>
{
private:
    DXGI_FORMAT GetDXGIFormat(int Channels, bool IsHDR);
    
public:
    ErrorCode LoadTexture(const std::string& FilePath, Texture& OutTextureInstance);
};