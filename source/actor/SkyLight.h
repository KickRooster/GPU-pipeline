#pragma once
#include <memory>
#include <string>
#include "Actor.h"
#include "../asset/CubemapTexture.h"
#include "../dx12/TextureProxy.h"
#include "../dx12/ConstantBuffer.h"
#include "../dx12/ConstantBufferProxy.h"

class SkyLight : public Actor
{
private:
    const std::string HDRFilePath = "kloppenheim_05_4k.hdr";

public:
    std::unique_ptr<CubemapTexture> IrradianceMap;
    std::unique_ptr<CubemapTextureProxy> IrradianceMapProxy;

    std::unique_ptr<CubemapTexture> PrefilteredMap;
    std::unique_ptr<CubemapTextureProxy> PrefilteredMapProxy;

    std::unique_ptr<Texture> BRDFLUT;
    std::unique_ptr<TextureProxy> BRDFLUTProxy;

    std::unique_ptr<SkyLightConstantBuffer> ConstantBufferInstance;
    std::unique_ptr<ConstantBufferProxy> ConstantBufferProxyInstance;
    
    SkyLight();
    std::string GetHDRFilePath() const;
    void Update(float DeltaTime) override;
    ConstantBufferProxy* GetConstantBufferProxy() const override;
    virtual ~SkyLight() override = default;
};
