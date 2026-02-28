#include "CubemapTexture.h"
#include <memory>
#include <algorithm>
#include <vector>
#include "../misc/Math.h"

using namespace DirectX;
using namespace std;

// Helper functions
inline float clamp(float value, float minVal, float maxVal)
{
    return max(minVal, min(maxVal, value));
}

inline int clamp(int value, int minVal, int maxVal)
{
    return max(minVal, min(maxVal, value));
}

CubemapTexture::CubeFace::CubeFace(CubeFace&& Other) noexcept
{
    Data = Other.Data;
    Other.Data = nullptr;
}

CubemapTexture::CubeFace& CubemapTexture::CubeFace::operator=(CubeFace&& Other) noexcept
{
    if (this != &Other)
    {
        if (Data)
        {
            free(Data);
        }
        Data = Other.Data;
        Other.Data = nullptr;
    }
    return *this;
}

CubemapTexture::CubeFace::~CubeFace()
{
    free(Data);
}

XMVECTOR CubemapTexture::GetCubemapDirection(ECubeFace Face, int X, int Y, int Size) const
{
    float U = (static_cast<float>(X) + 0.5f) / static_cast<float>(Size);
    float V = (static_cast<float>(Y) + 0.5f) / static_cast<float>(Size);
    U = 2.0f * U - 1.0f;  // u: [0,1] -> [-1,1] 
    V = 1.0f - 2.0f * V;  // v: [0,1] -> [1,-1] (flip Y for cubemap)

    XMVECTOR Direction;
    
    switch (Face)
    {
    case ECubeFace::PositiveX:
        Direction = XMVectorSet(1.0f, V, U, 0.0f);
        break;
    case ECubeFace::NegativeX:
        Direction = XMVectorSet(-1.0f, V, -U, 0.0f);
        break;
    case ECubeFace::PositiveY:
        Direction = XMVectorSet(U, 1.0f, -V, 0.0f);
        break;
    case ECubeFace::NegativeY:
        Direction = XMVectorSet(U, -1.0f, V, 0.0f);
        break;
    case ECubeFace::PositiveZ:
        Direction = XMVectorSet(U, V, 1.0f, 0.0f);
        break;
    case ECubeFace::NegativeZ:
        Direction = XMVectorSet(-U, V, -1.0f, 0.0f);
        break;
    }
    
    return XMVector3Normalize(Direction);
}

XMFLOAT2 CubemapTexture::DirectionToEquirectangularUV(const XMVECTOR& Direction) const
{
    XMFLOAT3 Direction3f;
    XMStoreFloat3(&Direction3f, Direction);
    
    // Convert 3D direction to spherical coordinates
    // Longitude: [-π, π] → [0, 1]
    float U = atan2f(Direction3f.z, Direction3f.x) / (2.0f * XM_PI) + 0.5f;
    
    // Latitude: [-π/2, π/2] → [0, 1] (standard mapping)
    float V = 0.5f - asinf(Direction3f.y) / XM_PI;
    
    return XMFLOAT2(U, V);
}

XMFLOAT3 CubemapTexture::SampleCubemap(int MipLevel, const XMVECTOR& Direction) const
{
    XMFLOAT3 Dir;
    XMStoreFloat3(&Dir, Direction);
    
    float AbsX = abs(Dir.x);
    float AbsY = abs(Dir.y);
    float AbsZ = abs(Dir.z);
    
    ECubeFace Face;
    float U;
    float V;
    
    if (AbsX >= AbsY && AbsX >= AbsZ)
    {
        if (Dir.x > 0.0f)
        {
            Face = ECubeFace::PositiveX;
            U = (-Dir.z / AbsX + 1.0f) * 0.5f;
            V = (-Dir.y / AbsX + 1.0f) * 0.5f;
        }
        else
        {
            Face = ECubeFace::NegativeX;
            U = (Dir.z / AbsX + 1.0f) * 0.5f;
            V = (-Dir.y / AbsX + 1.0f) * 0.5f;
        }
    }
    else if (AbsY >= AbsZ)
    {
        if (Dir.y > 0.0f)
        {
            Face = ECubeFace::PositiveY;
            U = (Dir.x / AbsY + 1.0f) * 0.5f;
            V = (Dir.z / AbsY + 1.0f) * 0.5f;
        }
        else
        {
            Face = ECubeFace::NegativeY;
            U = (Dir.x / AbsY + 1.0f) * 0.5f;
            V = (-Dir.z / AbsY + 1.0f) * 0.5f;
        }
    }
    else
    {
        if (Dir.z > 0.0f)
        {
            Face = ECubeFace::PositiveZ;
            U = (Dir.x / AbsZ + 1.0f) * 0.5f;
            V = (-Dir.y / AbsZ + 1.0f) * 0.5f;
        }
        else
        {
            Face = ECubeFace::NegativeZ;
            U = (-Dir.x / AbsZ + 1.0f) * 0.5f;
            V = (-Dir.y / AbsZ + 1.0f) * 0.5f;
        }
    }

    int Size = MipArray[MipLevel].Size;
    int X = clamp(static_cast<int>(U * Size), 0, Size - 1);
    int Y = clamp(static_cast<int>(V * Size), 0, Size - 1);

    const CubeFace* Faces = MipArray[MipLevel].Faces;
    const float* FaceData = static_cast<const float*>(Faces[static_cast<int>(Face)].Data);
    int PixelIndex = (Y * Size + X) * 3;
    
    return XMFLOAT3(FaceData[PixelIndex], FaceData[PixelIndex + 1], FaceData[PixelIndex + 2]);
}

float CubemapTexture::RadicalInverse_VdC(uint32_t Bits) const
{
    Bits = (Bits << 16u) | (Bits >> 16u);
    Bits = ((Bits & 0x55555555u) << 1u) | ((Bits & 0xAAAAAAAAu) >> 1u);
    Bits = ((Bits & 0x33333333u) << 2u) | ((Bits & 0xCCCCCCCCu) >> 2u);
    Bits = ((Bits & 0x0F0F0F0Fu) << 4u) | ((Bits & 0xF0F0F0F0u) >> 4u);
    Bits = ((Bits & 0x00FF00FFu) << 8u) | ((Bits & 0xFF00FF00u) >> 8u);
    
    return static_cast<float>(Bits) * 2.3283064365386963e-10f; // / 2^32
}

vector<XMFLOAT2> CubemapTexture::GenerateHammersleySequence(int SampleCount) const
{
    vector<XMFLOAT2> Sequence;
    Sequence.reserve(SampleCount);
    
    for (int I = 0; I < SampleCount; ++I)
    {
        float U1 = static_cast<float>(I) / static_cast<float>(SampleCount);
        float U2 = RadicalInverse_VdC(static_cast<uint32_t>(I));
        Sequence.emplace_back(U1, U2);
    }
    
    return Sequence;
}

XMVECTOR CubemapTexture::GenerateHemisphereSample(float U1, float U2, const XMVECTOR& Normal) const
{
    float CosTheta = sqrtf(U1);
    float SinTheta = sqrtf(1.0f - U1);
    float Phi = 2.0f * XM_PI * U2;
    
    XMFLOAT3 TangentSample;
    TangentSample.x = SinTheta * cosf(Phi);
    TangentSample.y = SinTheta * sinf(Phi);
    TangentSample.z = CosTheta;
    
    XMFLOAT3 N;
    XMStoreFloat3(&N, Normal);
    
    XMVECTOR Up = abs(N.z) < 0.999f ? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f) : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    XMVECTOR Right = XMVector3Normalize(XMVector3Cross(Up, Normal));
    Up = XMVector3Normalize(XMVector3Cross(Normal, Right));
    
    return XMVectorScale(Right, TangentSample.x) + 
           XMVectorScale(Up, TangentSample.y) + 
           XMVectorScale(Normal, TangentSample.z);
}

XMFLOAT3 CubemapTexture::ConvolveDiffuse(const XMVECTOR& Normal, const vector<XMFLOAT2>& HammersleySequence) const
{
    XMFLOAT3 Irradiance(0.0f, 0.0f, 0.0f);
    
    for (const auto& Sample : HammersleySequence)
    {
        XMVECTOR SampleDirection = GenerateHemisphereSample(Sample.x, Sample.y, Normal);
        XMFLOAT3 Color = SampleCubemap(0, SampleDirection);
        
        float NdotL = max(0.0f, XMVectorGetX(XMVector3Dot(Normal, SampleDirection)));
        
        Irradiance.x += Color.x * NdotL;
        Irradiance.y += Color.y * NdotL;
        Irradiance.z += Color.z * NdotL;
    }
    
    float Scale = XM_PI / static_cast<float>(HammersleySequence.size());
    Irradiance.x *= Scale;
    Irradiance.y *= Scale;
    Irradiance.z *= Scale;
    
    return Irradiance;
}

XMVECTOR CubemapTexture::ImportanceSampleGGX(XMFLOAT2 Xi, XMVECTOR N, float Roughness) const
{
    float a = Roughness * Roughness;
    
    float Phi = 2.0f * XM_PI * Xi.x;
    float CosTheta = sqrtf((1.0f - Xi.y) / (1.0f + (a*a - 1.0f) * Xi.y));
    float SinTheta = sqrtf(1.0f - CosTheta * CosTheta);
    
    // Spherical coordinates to cartesian in tangent space
    XMFLOAT3 H;
    H.x = cosf(Phi) * SinTheta;
    H.y = sinf(Phi) * SinTheta;
    H.z = CosTheta;
    
    // Transform from tangent space to world space
    XMFLOAT3 N3;
    XMStoreFloat3(&N3, N);
    
    XMVECTOR Up = abs(N3.z) < 0.999f ? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f) : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    XMVECTOR Tangent = XMVector3Normalize(XMVector3Cross(Up, N));
    XMVECTOR Bitangent = XMVector3Cross(N, Tangent);
    
    XMVECTOR SampleVec = XMVectorScale(Tangent, H.x) + XMVectorScale(Bitangent, H.y) + XMVectorScale(N, H.z);
    return XMVector3Normalize(SampleVec);
}

float CubemapTexture::DistributionGGX(XMVECTOR N, XMVECTOR H, float Roughness) const
{
    float a = Roughness * Roughness;
    float a2 = a * a;
    float NdotH = max(XMVectorGetX(XMVector3Dot(N, H)), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = XM_PI * denom * denom;

    return nom / denom;
}

float CubemapTexture::GeometrySchlickGGX(float NdotV, float Roughness) const
{
    float r = (Roughness + 1.0f);
    float k = (r * r) / 8.0f;

    float nom = NdotV;
    float denom = NdotV * (1.0f - k) + k;

    return nom / denom;
}

float CubemapTexture::GeometrySmith(XMVECTOR N, XMVECTOR V, XMVECTOR L, float Roughness) const
{
    float NdotV = max(XMVectorGetX(XMVector3Dot(N, V)), 0.0f);
    float NdotL = max(XMVectorGetX(XMVector3Dot(N, L)), 0.0f);
    float ggx2 = GeometrySchlickGGX(NdotV, Roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, Roughness);

    return ggx1 * ggx2;
}

XMFLOAT2 CubemapTexture::IntegrateBRDF(float NdotV, float Roughness, const std::vector<XMFLOAT2>& HammersleySequence) const
{
    XMVECTOR V = XMVectorSet(sqrtf(1.0f - NdotV*NdotV), 0.0f, NdotV, 0.0f);
    XMVECTOR N = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

    float A = 0.0f;
    float B = 0.0f;

    for (const auto& Sample : HammersleySequence)
    {
        float a = Roughness * Roughness;
        float Phi = 2.0f * XM_PI * Sample.x;
        float CosTheta = sqrtf((1.0f - Sample.y) / (1.0f + (a*a - 1.0f) * Sample.y));
        float SinTheta = sqrtf(1.0f - CosTheta*CosTheta);

        XMVECTOR H = XMVectorSet(cosf(Phi) * SinTheta, sinf(Phi) * SinTheta, CosTheta, 0.0f);
        XMVECTOR L = XMVector3Normalize(2.0f * XMVector3Dot(V, H) * H - V);

        float NdotL = max(XMVectorGetX(XMVector3Dot(N, L)), 0.0f);
        float NdotH = max(XMVectorGetX(XMVector3Dot(N, H)), 0.0f);
        float VdotH = max(XMVectorGetX(XMVector3Dot(V, H)), 0.0f);

        if (NdotL > 0.0f)
        {
            float G = GeometrySmith(N, V, L, Roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = powf(1.0f - VdotH, 5.0f);

            A += (1.0f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A /= static_cast<float>(HammersleySequence.size());
    B /= static_cast<float>(HammersleySequence.size());
    
    return XMFLOAT2(A, B);
}

void CubemapTexture::SetFaceData(int MipLevel, ECubeFace Face, void* Data)
{
    MipArray[MipLevel].Faces[static_cast<int>(Face)].Data = Data;
}

CubemapTexture::CubemapTexture(const Texture& HDRTexture)
{
    MipArray.resize(1);
    MipArray[0].Size = HDRTexture.GetWidth() / 4;
    IsHDR = HDRTexture.IsHDR;
    ChannelCount = HDRTexture.Channels;
    Format = HDRTexture.Format;
    
    const size_t FaceDataSize = MipArray[0].Size * MipArray[0].Size * ChannelCount * sizeof(float);
    
    for (int FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
    {
        ECubeFace Face = static_cast<ECubeFace>(FaceIndex);
        
        float* FaceData = static_cast<float*>(malloc(FaceDataSize));
        
        for (int Y = 0; Y < MipArray[0].Size; ++Y)
        {
            for (int X = 0; X < MipArray[0].Size; ++X)
            {
                XMVECTOR Direction = GetCubemapDirection(Face, X, Y, MipArray[0].Size);
                XMFLOAT2 EquirectangularUV = DirectionToEquirectangularUV(Direction);
                XMFLOAT4 Color = HDRTexture.Sample(EquirectangularUV);
                
                int PixelIndex = (Y * MipArray[0].Size + X) * ChannelCount;
                FaceData[PixelIndex] = Color.x;
                FaceData[PixelIndex + 1] = Color.y;
                FaceData[PixelIndex + 2] = Color.z;

                if (ChannelCount == 4)
                {
                    FaceData[PixelIndex + 3] = Color.w;
                }
            }
        }
        
        MipArray[0].Faces[FaceIndex].Data = FaceData;
    }
}

CubemapTexture::CubemapTexture(int MipLevels, int InBaseMipSize, int InChannelCount, bool InIsHDR, DXGI_FORMAT InFormat)
    :ChannelCount(InChannelCount),
    IsHDR(InIsHDR),
    Format(InFormat)
{
    MipArray.resize(MipLevels);
    for (int MipLevel = 0; MipLevel < MipLevels; ++MipLevel)
    {
        MipArray[MipLevel].Size = InBaseMipSize >> MipLevel;
    }
}

void* CubemapTexture::GetFaceData(int MipLevel, ECubeFace Face) const
{
    return MipArray[MipLevel].Faces[static_cast<int>(Face)].Data;
}

int CubemapTexture::GetSize(int MipLevel) const
{
    return MipArray[MipLevel].Size;
}

int CubemapTexture::GetMipLevels() const
{
    return static_cast<int>(MipArray.size());
}

int CubemapTexture::GetChannelCount() const
{
    return ChannelCount;
}

DXGI_FORMAT CubemapTexture::GetFormat() const
{
    return Format;
}

unique_ptr<CubemapTexture> CubemapTexture::Convolution(int IrradianceSize, int SampleCount) const
{
    static vector<XMFLOAT2> HammersleySequence = GenerateHammersleySequence(SampleCount);
    unique_ptr<CubemapTexture> OutIrradianceMap = make_unique<CubemapTexture>(1, IrradianceSize, 4, IsHDR, DXGI_FORMAT_R32G32B32A32_FLOAT);
    
    const size_t IrradianceFaceDataSize = IrradianceSize * IrradianceSize * OutIrradianceMap->GetChannelCount() * sizeof(float);
    
    for (int FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
    {
        ECubeFace Face = static_cast<ECubeFace>(FaceIndex);
        float* IrradianceFaceData = static_cast<float*>(malloc(IrradianceFaceDataSize));
        
        for (int Y = 0; Y < IrradianceSize; ++Y)
        {
            for (int X = 0; X < IrradianceSize; ++X)
            {
                XMVECTOR Normal = GetCubemapDirection(Face, X, Y, IrradianceSize);
                XMFLOAT3 Irradiance = ConvolveDiffuse(Normal, HammersleySequence);
                
                int PixelIndex = (Y * IrradianceSize + X) * OutIrradianceMap->GetChannelCount();
                IrradianceFaceData[PixelIndex] = Irradiance.x;
                IrradianceFaceData[PixelIndex + 1] = Irradiance.y;
                IrradianceFaceData[PixelIndex + 2] = Irradiance.z;
                
                if (OutIrradianceMap->GetChannelCount() == 4)
                {
                    IrradianceFaceData[PixelIndex + 3] = 1.0f;
                }
            }
        }

        OutIrradianceMap->SetFaceData(0, Face, IrradianceFaceData);
    }

    return OutIrradianceMap;
}

unique_ptr<CubemapTexture> CubemapTexture::PrefilterEnvironment(int MipLevels, int BaseMipSize, int SampleCount) const
{
    static vector<XMFLOAT2> HammersleySequence = GenerateHammersleySequence(SampleCount);
    
    // Create a new cubemap with multiple mip levels
    unique_ptr<CubemapTexture> PrefilteredCubemap = make_unique<CubemapTexture>(MipLevels, BaseMipSize, 4, IsHDR, DXGI_FORMAT_R32G32B32A32_FLOAT);

    for (int MipLevel = 0; MipLevel < MipLevels; ++MipLevel)
    {
        int MipSize = BaseMipSize >> MipLevel;
        
        float Roughness = static_cast<float>(MipLevel) / static_cast<float>(MipLevels - 1);
        
        const size_t MipFaceDataSize = MipSize * MipSize * PrefilteredCubemap->GetChannelCount() * sizeof(float);

        for (int FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
        {
            ECubeFace Face = static_cast<ECubeFace>(FaceIndex);
            float* MipFaceData = static_cast<float*>(malloc(MipFaceDataSize));

            for (int Y = 0; Y < MipSize; ++Y)
            {
                for (int X = 0; X < MipSize; ++X)
                {
                    XMVECTOR R = GetCubemapDirection(Face, X, Y, MipSize);
                    
                    XMFLOAT3 PrefilteredColor;
                    if (MipLevel == 0)
                    {
                        // Mip 0: copy original environment map
                        PrefilteredColor = SampleCubemap(0, R);
                    }
                    else
                    {
                        // Prefilter for this roughness level using importance sampling
                        XMVECTOR N = R;
                        XMVECTOR V = R;
                        PrefilteredColor = XMFLOAT3(0.0f, 0.0f, 0.0f);
                        float TotalWeight = 0.0f;

                        for (const auto& Sample : HammersleySequence)
                        {
                            XMVECTOR H = ImportanceSampleGGX(Sample, N, Roughness);
                            XMVECTOR L = XMVector3Normalize(2.0f * XMVector3Dot(V, H) * H - V);

                            float NdotL = max(XMVectorGetX(XMVector3Dot(N, L)), 0.0f);
                            
                            if (NdotL > 0.0f)
                            {
                                XMFLOAT3 SampleColor = SampleCubemap(0, L);
                                
                                PrefilteredColor.x += SampleColor.x * NdotL;
                                PrefilteredColor.y += SampleColor.y * NdotL;
                                PrefilteredColor.z += SampleColor.z * NdotL;
                                TotalWeight += NdotL;
                            }
                        }

                        if (TotalWeight > 0.0f)
                        {
                            PrefilteredColor.x /= TotalWeight;
                            PrefilteredColor.y /= TotalWeight;
                            PrefilteredColor.z /= TotalWeight;
                        }
                    }

                    int PixelIndex = (Y * MipSize + X) * PrefilteredCubemap->GetChannelCount();
                    MipFaceData[PixelIndex] = PrefilteredColor.x;
                    MipFaceData[PixelIndex + 1] = PrefilteredColor.y;
                    MipFaceData[PixelIndex + 2] = PrefilteredColor.z;
                    
                    if (PrefilteredCubemap->GetChannelCount() == 4)
                    {
                        MipFaceData[PixelIndex + 3] = 1.0f;
                    }
                }
            }

            PrefilteredCubemap->SetFaceData(MipLevel, Face, MipFaceData);
        }
    }

    return PrefilteredCubemap;
}

unique_ptr<Texture> CubemapTexture::GenerateBRDFLUT(int Resolution, int SampleCount) const
{
    static vector<XMFLOAT2> HammersleySequence = GenerateHammersleySequence(SampleCount);

    const size_t DataSize = Resolution * Resolution * 4 * sizeof(float);
    float* LutData = static_cast<float*>(malloc(DataSize));

    for (int Y = 0; Y < Resolution; ++Y)
    {
        for (int X = 0; X < Resolution; ++X)
        {
            float NdotV = (static_cast<float>(X) + 0.5f) / static_cast<float>(Resolution);
            float Roughness = (static_cast<float>(Y) + 0.5f) / static_cast<float>(Resolution);
            
            XMFLOAT2 IntegratedBRDF = IntegrateBRDF(NdotV, Roughness, HammersleySequence);
            
            int PixelIndex = (Y * Resolution + X) * 4;
            LutData[PixelIndex] = IntegratedBRDF.x;     // R
            LutData[PixelIndex + 1] = IntegratedBRDF.y; // G
            LutData[PixelIndex + 2] = 0.0f;             // B
            LutData[PixelIndex + 3] = 1.0f;             // A
        }
    }
    
    unique_ptr<Texture> BrdfLut = make_unique<Texture>();
    BrdfLut->Channels = 4;
    BrdfLut->IsHDR = true;
    BrdfLut->Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    MipLevel BaseMip;
    BaseMip.Width = Resolution;
    BaseMip.Height = Resolution;
    size_t ByteSize = Resolution * Resolution * 4 * sizeof(float);
    BaseMip.Data.resize(ByteSize);
    memcpy(BaseMip.Data.data(), LutData, ByteSize);
    free(LutData);
    BrdfLut->Mips.push_back(std::move(BaseMip));
    
    return BrdfLut;
}

CubemapTexture::~CubemapTexture()
{
}