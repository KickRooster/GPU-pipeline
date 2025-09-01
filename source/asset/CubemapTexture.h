#pragma once
#include "Texture.h"
#include "../misc/Math.h"
#include <vector>
#include <memory>

enum class ECubeFace : int
{
    PositiveX = 0, // +X (Right)
    NegativeX = 1, // -X (Left)
    PositiveY = 2, // +Y (Top)
    NegativeY = 3, // -Y (Bottom)
    PositiveZ = 4, // +Z (Front)
    NegativeZ = 5, // -Z (Back)
};

class CubemapTexture
{
public:
    struct CubeFace
    {
        void* Data = nullptr;
        
        CubeFace() = default;
        CubeFace(CubeFace&& Other) noexcept;
        CubeFace& operator=(CubeFace&& Other) noexcept;
        CubeFace(const CubeFace&) = delete;
        CubeFace& operator=(const CubeFace&) = delete;
        ~CubeFace();
    };

    struct MipData
    {
        CubeFace Faces[6];
        int Size = 0;
    };

private:
    std::vector<MipData> MipArray;
    int ChannelCount;
    bool IsHDR;
    DXGI_FORMAT Format;
    
    DirectX::XMVECTOR GetCubemapDirection(ECubeFace Face, int X, int Y, int Size) const;
    DirectX::XMFLOAT2 DirectionToEquirectangularUV(const DirectX::XMVECTOR& Direction) const;
    DirectX::XMFLOAT3 SampleCubemap(int MipLevel, const DirectX::XMVECTOR& Direction) const;
    float RadicalInverse_VdC(uint32_t Bits) const;
    std::vector<DirectX::XMFLOAT2> GenerateHammersleySequence(int SampleCount) const;
    DirectX::XMVECTOR GenerateHemisphereSample(float U1, float U2, const DirectX::XMVECTOR& Normal) const;
    DirectX::XMFLOAT3 ConvolveDiffuse(const DirectX::XMVECTOR& Normal, const std::vector<DirectX::XMFLOAT2>& HammersleySequence) const;
    DirectX::XMVECTOR ImportanceSampleGGX(DirectX::XMFLOAT2 Xi, DirectX::XMVECTOR N, float Roughness) const;
    float DistributionGGX(DirectX::XMVECTOR N, DirectX::XMVECTOR H, float Roughness) const;
    float GeometrySchlickGGX(float NdotV, float Roughness) const;
    float GeometrySmith(DirectX::XMVECTOR N, DirectX::XMVECTOR V, DirectX::XMVECTOR L, float Roughness) const;
    DirectX::XMFLOAT2 IntegrateBRDF(float NdotV, float Roughness, const std::vector<DirectX::XMFLOAT2>& HammersleySequence) const;
    void SetFaceData(int MipLevel, ECubeFace Face, void* Data);
    
public:
    CubemapTexture(const Texture& HDRTexture);
    CubemapTexture(int MipLevels, int InBaseMipSize, int InChannelCount, bool InIsHDR, DXGI_FORMAT InFormat);
    CubemapTexture(const CubemapTexture&) = delete;
    CubemapTexture& operator=(const CubemapTexture&) = delete;
    CubemapTexture(CubemapTexture&&) = default;
    CubemapTexture& operator=(CubemapTexture&&) = default;
    void* GetFaceData(int MipLevel, ECubeFace Face) const;
    int GetSize(int MipLevel) const;
    int GetMipLevels() const;
    int GetChannelCount() const;
    DXGI_FORMAT GetFormat() const;
    std::unique_ptr<CubemapTexture> Convolution(int IrradianceSize, int SampleCount) const;
    std::unique_ptr<CubemapTexture> PrefilterEnvironment(int MipLevels, int BaseMipSize, int SampleCount) const;
    std::unique_ptr<Texture> GenerateBRDFLUT(int Resolution, int SampleCount) const;
    ~CubemapTexture();
};