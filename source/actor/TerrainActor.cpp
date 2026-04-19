#include "TerrainActor.h"
#include "../asset/TextureLoader.h"
#include "../dx12/PipelineInterface.h"
#include <cmath>

using namespace std;

TerrainMaterialTextures::TerrainMaterialTextures()
{
    for (unsigned int LayerIndex = 0; LayerIndex < TERRAIN_MAX_LAYERS; ++LayerIndex)
    {
        AlbedoIndices[LayerIndex] = 0xFFFFFFFF;
        NormalIndices[LayerIndex] = 0xFFFFFFFF;
        RoughnessIndices[LayerIndex] = 0xFFFFFFFF;
    }
}

TerrainProxy* TerrainActor::GetProxy() const
{
    return ProxyInstance.get();
}

void TerrainActor::BuildQuadTree()
{
    unsigned int TotalNodes = 0;
    unsigned int Power4 = 1;
    for (unsigned int L = 0; L <= MaxLevel; ++L)
    {
        TotalNodes += Power4;
        Power4 *= 4;
    }

    const unsigned int NonLeafCount = TotalNodes - PatchCount;
    Bounds.resize(PatchCount + NonLeafCount);

    QuadTreeNodes.resize(TotalNodes);

    std::vector<unsigned int> LevelStart(MaxLevel + 1);
    Power4 = 1;
    for (unsigned int L = 0; L <= MaxLevel; ++L)
    {
        LevelStart[L] = (Power4 - 1) / 3;
        Power4 *= 4;
    }

    const unsigned int LeafStart = LevelStart[MaxLevel];
    for (unsigned int LeafIdx = 0; LeafIdx < PatchCount; ++LeafIdx)
    {
        QuadTreeNode& Node = QuadTreeNodes[LeafStart + LeafIdx];
        Node.Level = MaxLevel;
        Node.ChildIndices[0] = 0;
        Node.ChildIndices[1] = 0;
        Node.ChildIndices[2] = 0;
        Node.ChildIndices[3] = 0;
        Node.BoundIndex = LeafIdx;
        Node.PatchIndex = LeafIdx;
    }

    unsigned int NonLeafBoundIdx = PatchCount;
    for (int L = static_cast<int>(MaxLevel) - 1; L >= 0; --L)
    {
        const unsigned int ParentStart = LevelStart[L];
        const unsigned int ChildStart = LevelStart[L + 1];
        const unsigned int ParentGridSize = 1u << L;
        const unsigned int ChildGridSize = 1u << (L + 1);

        for (unsigned int Row = 0; Row < ParentGridSize; ++Row)
        {
            for (unsigned int Col = 0; Col < ParentGridSize; ++Col)
            {
                const unsigned int ParentIdx = Row * ParentGridSize + Col;
                QuadTreeNode& Parent = QuadTreeNodes[ParentStart + ParentIdx];
                Parent.Level = L;
                Parent.PatchIndex = 0;

                const unsigned int C0 = ChildStart + (2 * Row)     * ChildGridSize + (2 * Col);
                const unsigned int C1 = ChildStart + (2 * Row)     * ChildGridSize + (2 * Col + 1);
                const unsigned int C2 = ChildStart + (2 * Row + 1) * ChildGridSize + (2 * Col);
                const unsigned int C3 = ChildStart + (2 * Row + 1) * ChildGridSize + (2 * Col + 1);
                Parent.ChildIndices[0] = C0;
                Parent.ChildIndices[1] = C1;
                Parent.ChildIndices[2] = C2;
                Parent.ChildIndices[3] = C3;

                float MinX = 1e10f;
                float MinY = 1e10f;
                float MinZ = 1e10f;
                float MaxX = -1e10f;
                float MaxY = -1e10f;
                float MaxZ = -1e10f;

                const unsigned int Children[4] = { C0, C1, C2, C3 };
                for (int C = 0; C < 4; ++C)
                {
                    const QuadTreeNode& Child = QuadTreeNodes[Children[C]];
                    const TerrainPatchBound& ChildBound = Bounds[Child.BoundIndex];

                    MinX = (ChildBound.Center[0] - ChildBound.HalfExtent[0] < MinX) ? ChildBound.Center[0] - ChildBound.HalfExtent[0] : MinX;
                    MinY = (ChildBound.Center[1] - ChildBound.HalfExtent[1] < MinY) ? ChildBound.Center[1] - ChildBound.HalfExtent[1] : MinY;
                    MinZ = (ChildBound.Center[2] - ChildBound.HalfExtent[2] < MinZ) ? ChildBound.Center[2] - ChildBound.HalfExtent[2] : MinZ;
                    MaxX = (ChildBound.Center[0] + ChildBound.HalfExtent[0] > MaxX) ? ChildBound.Center[0] + ChildBound.HalfExtent[0] : MaxX;
                    MaxY = (ChildBound.Center[1] + ChildBound.HalfExtent[1] > MaxY) ? ChildBound.Center[1] + ChildBound.HalfExtent[1] : MaxY;
                    MaxZ = (ChildBound.Center[2] + ChildBound.HalfExtent[2] > MaxZ) ? ChildBound.Center[2] + ChildBound.HalfExtent[2] : MaxZ;
                }

                TerrainPatchBound& ParentBound = Bounds[NonLeafBoundIdx];
                ParentBound.Center[0] = (MinX + MaxX) * 0.5f;
                ParentBound.Center[1] = (MinY + MaxY) * 0.5f;
                ParentBound.Center[2] = (MinZ + MaxZ) * 0.5f;
                ParentBound.HalfExtent[0] = (MaxX - MinX) * 0.5f;
                float ParentHalfY = (MaxY - MinY) * 0.5f;
                float ParentPatchSize = PatchSize * static_cast<float>(1u << (MaxLevel - static_cast<unsigned int>(L)));
                ParentBound.HalfExtent[1] = std::max(ParentHalfY, ParentPatchSize * 0.5f);
                ParentBound.HalfExtent[2] = (MaxZ - MinZ) * 0.5f;

                Parent.BoundIndex = NonLeafBoundIdx;
                NonLeafBoundIdx++;
            }
        }
    }

    for (int L = static_cast<int>(MaxLevel) - 1; L >= 0; --L)
    {
        const unsigned int NodesAtLevel = (1u << L) * (1u << L);
        const float NodePatchSize = PatchSize * static_cast<float>(1u << (MaxLevel - static_cast<unsigned int>(L)));

        for (unsigned int N = 0; N < NodesAtLevel; ++N)
        {
            QuadTreeNode& Node = QuadTreeNodes[LevelStart[L] + N];
            const TerrainPatchBound& B = Bounds[Node.BoundIndex];

            TerrainPatchData PatchData;
            PatchData.WorldOffsetX = B.Center[0] - B.HalfExtent[0];
            PatchData.WorldOffsetZ = B.Center[2] - B.HalfExtent[2];
            PatchData.PatchSize = NodePatchSize;
            PatchData.PatchIndex = static_cast<unsigned int>(Patches.size());
            PatchData.LodLevel = MaxLevel - static_cast<unsigned int>(L);

            Node.PatchIndex = PatchData.PatchIndex;
            Patches.push_back(PatchData);
        }
    }

    TotalNodeCount = static_cast<unsigned int>(Patches.size());
}

void TerrainActor::SelectActivePatches(const DirectX::XMFLOAT3& CameraPos)
{
    std::vector<unsigned int> NewIndices;

    if (QuadTreeNodes.empty())
    {
        if (!ActivePatchIndices.empty())
        {
            ActivePatchIndices.clear();
            ActivePatchIndicesDirty = true;
        }
        return;
    }

    const float InvScale = (DebugScale > 1e-3f) ? (1.0f / DebugScale) : 1.0f;
    const DirectX::XMFLOAT3 ScaledCamPos = { CameraPos.x * InvScale, CameraPos.y * InvScale, CameraPos.z * InvScale };

    const float NearPatchSize = (TargetNearPatchSize > 1e-3f) ? TargetNearPatchSize : PatchSize;

    std::vector<unsigned int> Stack;
    Stack.reserve(256);
    Stack.push_back(0);

    while (!Stack.empty())
    {
        const unsigned int NodeIdx = Stack.back();
        Stack.pop_back();
        const QuadTreeNode& Node = QuadTreeNodes[NodeIdx];

        const bool IsLeaf = (Node.ChildIndices[0] == 0);
        if (IsLeaf)
        {
            NewIndices.push_back(Node.PatchIndex);
            continue;
        }

        const unsigned int LodLevel = MaxLevel - Node.Level;
        const float NodePatchSize = PatchSize * static_cast<float>(1u << LodLevel);

        if (NodePatchSize <= NearPatchSize)
        {
            NewIndices.push_back(Node.PatchIndex);
            continue;
        }

        const TerrainPatchBound& B = Bounds[Node.BoundIndex];
        const float CenterX = B.Center[0];
        const float CenterZ = B.Center[2];

        const float DX = CenterX - ScaledCamPos.x;
        const float DZ = CenterZ - ScaledCamPos.z;
        const float DistSq = DX * DX + DZ * DZ;

        if (DistSq < LODRangesSq[LodLevel])
        {
            Stack.push_back(Node.ChildIndices[0]);
            Stack.push_back(Node.ChildIndices[1]);
            Stack.push_back(Node.ChildIndices[2]);
            Stack.push_back(Node.ChildIndices[3]);
        }
        else
        {
            NewIndices.push_back(Node.PatchIndex);
        }
    }

    if (NewIndices != ActivePatchIndices)
    {
        ActivePatchIndices = std::move(NewIndices);
        ActivePatchIndicesDirty = true;
    }

    for (unsigned int I = 0; I < ActivePatchIndices.size(); ++I)
    {
        TerrainPatchData& P = Patches[ActivePatchIndices[I]];
        const float PMinX = P.WorldOffsetX;
        const float PMaxX = P.WorldOffsetX + P.PatchSize;
        const float PMinZ = P.WorldOffsetZ;
        const float PMaxZ = P.WorldOffsetZ + P.PatchSize;

        const float Epsilon = PatchSize * 0.01f;
        float ProbeTopX = (PMinX + PMaxX) * 0.5f;
        float ProbeTopZ = PMinZ - Epsilon;
        float ProbeBottomX = (PMinX + PMaxX) * 0.5f;
        float ProbeBottomZ = PMaxZ + Epsilon;
        float ProbeLeftX = PMinX - Epsilon;
        float ProbeLeftZ = (PMinZ + PMaxZ) * 0.5f;
        float ProbeRightX = PMaxX + Epsilon;
        float ProbeRightZ = (PMinZ + PMaxZ) * 0.5f;

        unsigned char Top = 0xFF;
        unsigned char Bottom = 0xFF;
        unsigned char Left = 0xFF;
        unsigned char Right = 0xFF;

        for (unsigned int J = 0; J < ActivePatchIndices.size(); ++J)
        {
            if (J == I)
            {
                continue;
            }

            const TerrainPatchData& Q = Patches[ActivePatchIndices[J]];
            if (Q.LodLevel <= P.LodLevel)
            {
                continue;
            }

            const float QMinX = Q.WorldOffsetX;
            const float QMaxX = Q.WorldOffsetX + Q.PatchSize;
            const float QMinZ = Q.WorldOffsetZ;
            const float QMaxZ = Q.WorldOffsetZ + Q.PatchSize;

            if (Top == 0xFF && ProbeTopX >= QMinX && ProbeTopX < QMaxX && ProbeTopZ >= QMinZ && ProbeTopZ < QMaxZ)
            {
                Top = static_cast<unsigned char>(Q.LodLevel);
            }
            if (Bottom == 0xFF && ProbeBottomX >= QMinX && ProbeBottomX < QMaxX && ProbeBottomZ >= QMinZ && ProbeBottomZ < QMaxZ)
            {
                Bottom = static_cast<unsigned char>(Q.LodLevel);
            }
            if (Left == 0xFF && ProbeLeftX >= QMinX && ProbeLeftX < QMaxX && ProbeLeftZ >= QMinZ && ProbeLeftZ < QMaxZ)
            {
                Left = static_cast<unsigned char>(Q.LodLevel);
            }
            if (Right == 0xFF && ProbeRightX >= QMinX && ProbeRightX < QMaxX && ProbeRightZ >= QMinZ && ProbeRightZ < QMaxZ)
            {
                Right = static_cast<unsigned char>(Q.LodLevel);
            }

            if (Top != 0xFF && Bottom != 0xFF && Left != 0xFF && Right != 0xFF)
            {
                break;
            }
        }

        const unsigned int Packed = static_cast<unsigned int>(Top)
                                  | (static_cast<unsigned int>(Bottom) << 8)
                                  | (static_cast<unsigned int>(Left)   << 16)
                                  | (static_cast<unsigned int>(Right)  << 24);

        P.NeighborLodPacked = Packed;
    }
}

ErrorCode TerrainActor::Initialize(const std::string& HeightmapPath)
{
    this->HeightmapPath = HeightmapPath;

    GridCount = static_cast<unsigned int>(TerrainSize / TargetNearPatchSize);

    if ((GridCount & (GridCount - 1)) != 0)
    {
        unsigned int PowerOf2 = 1;
        while (PowerOf2 < GridCount)
        {
            PowerOf2 <<= 1;
        }
        GridCount = PowerOf2;
    }

    HeightmapTexture = std::make_unique<Texture>();
    ErrorCode Result = TextureLoader::GetInstance().LoadTexture(HeightmapPath, false, *HeightmapTexture);
    if (Result != ErrorCode::OK)
    {
        HeightmapTexture.reset();
        return Result;
    }

    const int Width = HeightmapTexture->GetWidth();
    const int Height = HeightmapTexture->GetHeight();
    const int Channels = HeightmapTexture->Channels;
    const bool IsHDR = HeightmapTexture->IsHDR;
    const bool Is16Bit = HeightmapTexture->Is16Bit;
    const void* HeightmapData = HeightmapTexture->GetData();

    PatchCount = GridCount * GridCount;
    PatchSize = TerrainSize / static_cast<float>(GridCount);

    MaxLevel = static_cast<unsigned int>(log2(GridCount));
    if (MaxLevel >= MAX_LOD_LEVELS)
    {
        char Buffer[256];
        sprintf_s(Buffer, "ERROR: GridCount=%u requires MaxLevel=%u, but MAX_LOD_LEVELS=%u.\n",
            GridCount, MaxLevel, MAX_LOD_LEVELS);
        OutputDebugStringA(Buffer);
        return ErrorCode::ExceedMaxLODLevels;
    }

    const float HalfTerrain = TerrainSize * 0.5f;
    const float InvTerrainSize = 1.0f / TerrainSize;

    Patches.resize(PatchCount);
    for (unsigned int Row = 0; Row < GridCount; ++Row)
    {
        for (unsigned int Col = 0; Col < GridCount; ++Col)
        {
            unsigned int Index = Row * GridCount + Col;
            Patches[Index].WorldOffsetX = static_cast<float>(Col) * PatchSize - HalfTerrain;
            Patches[Index].WorldOffsetZ = static_cast<float>(Row) * PatchSize - HalfTerrain;
            Patches[Index].PatchSize = PatchSize;
            Patches[Index].PatchIndex = Index;
            Patches[Index].LodLevel = 0;
        }
    }

    Bounds.resize(PatchCount);
    for (unsigned int PatchIdx = 0; PatchIdx < PatchCount; ++PatchIdx)
    {
        const TerrainPatchData& Patch = Patches[PatchIdx];

        float UMin = (Patch.WorldOffsetX + HalfTerrain) * InvTerrainSize;
        float UMax = (Patch.WorldOffsetX + Patch.PatchSize + HalfTerrain) * InvTerrainSize;
        float VMin = (Patch.WorldOffsetZ + HalfTerrain) * InvTerrainSize;
        float VMax = (Patch.WorldOffsetZ + Patch.PatchSize + HalfTerrain) * InvTerrainSize;

        UMin = (UMin < 0.0f) ? 0.0f : ((UMin > 1.0f) ? 1.0f : UMin);
        UMax = (UMax < 0.0f) ? 0.0f : ((UMax > 1.0f) ? 1.0f : UMax);
        VMin = (VMin < 0.0f) ? 0.0f : ((VMin > 1.0f) ? 1.0f : VMin);
        VMax = (VMax < 0.0f) ? 0.0f : ((VMax > 1.0f) ? 1.0f : VMax);

        const int TexelXMin = std::max(static_cast<int>(std::floor(UMin * (Width - 1))) - 1, 0);
        const int TexelXMax = std::min(static_cast<int>(std::ceil(UMax * (Width - 1))) + 1, Width - 1);
        const int TexelYMin = std::max(static_cast<int>(std::floor(VMin * (Height - 1))) - 1, 0);
        const int TexelYMax = std::min(static_cast<int>(std::ceil(VMax * (Height - 1))) + 1, Height - 1);

        float MinH = 1.0f;
        float MaxH = 0.0f;
        for (int Y = TexelYMin; Y <= TexelYMax; ++Y)
        {
            for (int X = TexelXMin; X <= TexelXMax; ++X)
            {
                float H = 0.0f;
                const int TexelIdx = Y * Width + X;
                if (IsHDR)
                {
                    const float* Pixels = static_cast<const float*>(HeightmapData);
                    H = Pixels[TexelIdx * Channels];
                }
                else if (Is16Bit)
                {
                    const unsigned short* Pixels = static_cast<const unsigned short*>(HeightmapData);
                    H = static_cast<float>(Pixels[TexelIdx * Channels]) / 65535.0f;
                }
                else
                {
                    const unsigned char* Pixels = static_cast<const unsigned char*>(HeightmapData);
                    H = static_cast<float>(Pixels[TexelIdx * Channels]) / 255.0f;
                }

                MinH = min(H, MinH);
                MaxH = max(H, MaxH);
            }
        }

        const float MinWorldHeight = MinH * HeightScale;
        const float MaxWorldHeight = MaxH * HeightScale;
        const float CenterY = (MinWorldHeight + MaxWorldHeight) * 0.5f;
        const float MinHalfExtentY = PatchSize * 0.5f;
        const float HalfExtentY = std::max((MaxWorldHeight - MinWorldHeight) * 0.5f, MinHalfExtentY);
        const float HalfPatch = Patch.PatchSize * 0.5f;

        Bounds[PatchIdx].Center[0] = Patch.WorldOffsetX + HalfPatch;
        Bounds[PatchIdx].Center[1] = CenterY;
        Bounds[PatchIdx].Center[2] = Patch.WorldOffsetZ + HalfPatch;
        Bounds[PatchIdx].HalfExtent[0] = HalfPatch;
        Bounds[PatchIdx].HalfExtent[1] = HalfExtentY;
        Bounds[PatchIdx].HalfExtent[2] = HalfPatch;
    }

    BuildQuadTree();

    const float BaseDist = PatchSize * 2.0f;
    for (unsigned int K = 0; K < MAX_LOD_LEVELS; ++K)
    {
        const float R = BaseDist * static_cast<float>(1u << K);
        LODRangesSq[K] = R * R;
    }

    ActivePatchIndices.resize(PatchCount);
    for (unsigned int PatchIndex = 0; PatchIndex < PatchCount; ++PatchIndex)
    {
        ActivePatchIndices[PatchIndex] = PatchIndex;
    }

    return ErrorCode::OK;
}

ErrorCode TerrainActor::LoadTerrainTextures()
{
    static const char* LayerNames[TERRAIN_MAX_LAYERS] = {
        "grass", "rock", "sand", "snow", "dirt", "gravel",
        "moss", "clay", "drygrass", "cliff", "mud", "ice"
    };

    auto& Pipeline = PipelineInterface::GetInstance();
    auto& Allocator = Pipeline.GetTextureBindlessAllocator();

    auto LoadTexture = [&](const char* Path, bool IsSRGB) -> unsigned int
    {
        auto TextureInstance = std::make_unique<Texture>();
        ErrorCode Result = TextureLoader::GetInstance().LoadTexture(Path, IsSRGB, *TextureInstance);
        if (Result != ErrorCode::OK)
        {
            char Buffer[256];
            sprintf_s(Buffer, "WARNING: Failed to load terrain texture: %s\n", Path);
            OutputDebugStringA(Buffer);
            return 0xFFFFFFFF;
        }
        unsigned int DescriptorIndex = Allocator.AllocateRange(1);
        auto Proxy = std::make_unique<TextureProxy>();
        Pipeline.CreateTexture(TextureInstance.get(), DescriptorIndex, Proxy.get(), true);
        MaterialTextureProxies.push_back(std::move(Proxy));
        return DescriptorIndex;
    };

    unsigned int LoadedLayers = 0;
    for (unsigned int LayerIndex = 0; LayerIndex < TERRAIN_MAX_LAYERS; ++LayerIndex)
    {
        char AlbedoPath[128];
        char NormalPath[128];
        char RoughnessPath[128];
        sprintf_s(AlbedoPath, "terrain_%s_albedo.png", LayerNames[LayerIndex]);
        sprintf_s(NormalPath, "terrain_%s_normal.png", LayerNames[LayerIndex]);
        sprintf_s(RoughnessPath, "terrain_%s_roughness.png", LayerNames[LayerIndex]);

        unsigned int AlbedoIndex = LoadTexture(AlbedoPath, true);
        if (AlbedoIndex == 0xFFFFFFFF)
        {
            break;
        }

        MaterialTextures.AlbedoIndices[LayerIndex] = AlbedoIndex;
        MaterialTextures.NormalIndices[LayerIndex] = LoadTexture(NormalPath, false);
        MaterialTextures.RoughnessIndices[LayerIndex] = LoadTexture(RoughnessPath, false);
        LoadedLayers = LayerIndex + 1;
    }
    MaterialTextures.LayerCount = LoadedLayers;
    
    const char* HeightmapLoadPath = HeightmapPath.empty() ? "heightmap_4k.png" : HeightmapPath.c_str();
    MaterialTextures.HeightmapIndex = LoadTexture(HeightmapLoadPath, false);

    static const char* SplatPaths[3] = {
        "terrain_splatmap0.png", "terrain_splatmap1.png", "terrain_splatmap2.png"
    };
    static constexpr unsigned int LAYERS_PER_SPLATMAP = 4;
    unsigned int SplatCount = (LoadedLayers + LAYERS_PER_SPLATMAP - 1) / LAYERS_PER_SPLATMAP;
    for (unsigned int SplatIndex = 0; SplatIndex < SplatCount && SplatIndex < 3; ++SplatIndex)
    {
        MaterialTextures.SplatmapIndices[SplatIndex] = LoadTexture(SplatPaths[SplatIndex], false);
    }

    return ErrorCode::OK;
}

void TerrainActor::Update(float DeltaTime, unsigned int FrameIndex)
{
}

void TerrainActor::UpdateLOD(const DirectX::XMFLOAT3& CameraPos)
{
    SelectActivePatches(CameraPos);
}