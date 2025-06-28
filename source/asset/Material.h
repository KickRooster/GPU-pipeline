#pragma once
#include "../misc/Base.h"
#include "Texture.h"
#include "../dx12/TextureProxy.h"
#include <memory>

struct Material
{
    std::unique_ptr<Texture> AlbedoTexture;
    std::unique_ptr<Texture> NormalTexture;
    std::unique_ptr<Texture> MetallicTexture;
    std::unique_ptr<Texture> RoughnessTexture;

    std::unique_ptr<TextureProxy> AlbedoTextureProxy;
    std::unique_ptr<TextureProxy> NormalTextureProxy;
    std::unique_ptr<TextureProxy> MetallicTextureProxy;
    std::unique_ptr<TextureProxy> RoughnessTextureProxy;
};