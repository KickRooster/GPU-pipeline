#include "MeshLoader.h"
#include "Texture.h"
#include "../misc/Math.h"
#include "DirectXMath.h"
#include <algorithm>
#include <map>
#include <set>
#include <float.h>
#include <stdio.h>
#include <Windows.h>
#include <assimp/postprocess.h>

// Helper function to output formatted text to IDE output window
static void DebugPrintf(const char* format, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    OutputDebugStringA(buffer);
}

#define CLUSTERLOD_IMPLEMENTATION
#include "../../thirdpatry/meshoptimizer/src/meshoptimizer.h"
#include "../../thirdpatry/meshoptimizer/demo/clusterlod.h"

using namespace std;
using namespace DirectX;

// computes approximate (perspective) projection error of a cluster in screen space (0..1; multiply by screen height to get pixels)
// camera_proj is projection[1][1], or cot(fovy/2); camera_znear is *positive* near plane distance
// for DAG cut to be valid, boundsError must be monotonic: it must return a larger error for parent cluster
// for simplicity, we ignore perspective distortion and use rotationally invariant projection size estimation
static float boundsError(const clodBounds& bounds, float camera_x, float camera_y, float camera_z, float camera_proj, float camera_znear)
{
    float dx = bounds.center[0] - camera_x, dy = bounds.center[1] - camera_y, dz = bounds.center[2] - camera_z;
    float d = sqrtf(dx * dx + dy * dy + dz * dz) - bounds.radius;
    return bounds.error / (d > camera_znear ? d : camera_znear) * (camera_proj * 0.5f);
}

void MeshLoader::ExtractPBRTextures(const aiMaterial* Material, PBRTextureNamesPatch& OutTextureNamesPatch)
{
    aiString Path;
    
    if (Material->GetTexture(aiTextureType_DIFFUSE, 0, &Path) == AI_SUCCESS)
    {
        OutTextureNamesPatch.AlbedoPath = Path.C_Str();
    }
    else if (Material->GetTexture(aiTextureType_BASE_COLOR, 0, &Path) == AI_SUCCESS)
    {
        OutTextureNamesPatch.AlbedoPath = Path.C_Str();
    }
    
    if (Material->GetTexture(aiTextureType_NORMALS, 0, &Path) == AI_SUCCESS)
    {
        OutTextureNamesPatch.NormalPath = Path.C_Str();
    }
    else if (Material->GetTexture(aiTextureType_HEIGHT, 0, &Path) == AI_SUCCESS)
    {
        OutTextureNamesPatch.NormalPath = Path.C_Str();
    }
    //  XXX:    For fast develop only.
    else if (OutTextureNamesPatch.AlbedoPath.find("Cerberus_A.tga") != std::string::npos)
    {
        OutTextureNamesPatch.NormalPath = "Textures\\Cerberus_N.tga";
    }
    
    if (Material->GetTexture(aiTextureType_METALNESS, 0, &Path) == AI_SUCCESS)
    {
        OutTextureNamesPatch.MetallicPath = Path.C_Str();
    }
    //  XXX:    For fast develop only.
    else if (OutTextureNamesPatch.AlbedoPath.find("Cerberus_A.tga") != std::string::npos)
    {
        OutTextureNamesPatch.MetallicPath = "Textures\\Cerberus_M.tga";
    }
    
    if (Material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &Path) == AI_SUCCESS)
    {
        OutTextureNamesPatch.RoughnessPath = Path.C_Str();
    }
    //  XXX:    For fast develop only.
    else if (OutTextureNamesPatch.AlbedoPath.find("Cerberus_A.tga") != std::string::npos)
    {
        OutTextureNamesPatch.RoughnessPath = "Textures\\Cerberus_R.tga";
    }
    
    aiTextureType OtherTypes[] = {
        aiTextureType_SPECULAR,
        aiTextureType_AMBIENT,
        aiTextureType_EMISSIVE,
        aiTextureType_SHININESS,
        aiTextureType_OPACITY,
        aiTextureType_DISPLACEMENT,
        aiTextureType_LIGHTMAP,
        aiTextureType_REFLECTION,
        aiTextureType_BASE_COLOR,
        aiTextureType_NORMAL_CAMERA,
        aiTextureType_EMISSION_COLOR,
        aiTextureType_AMBIENT_OCCLUSION,
        aiTextureType_SHEEN,
        aiTextureType_CLEARCOAT,
        aiTextureType_TRANSMISSION,
        aiTextureType_MAYA_BASE,
        aiTextureType_MAYA_SPECULAR,
        aiTextureType_MAYA_SPECULAR_COLOR,
        aiTextureType_MAYA_SPECULAR_ROUGHNESS,
        aiTextureType_ANISOTROPY,
        aiTextureType_GLTF_METALLIC_ROUGHNESS,
        aiTextureType_UNKNOWN
    };
    
    for (aiTextureType Type : OtherTypes)
    {
        if (Material->GetTexture(Type, 0, &Path) == AI_SUCCESS)
        {
            OutTextureNamesPatch.OtherTexturePaths.push_back(Path.C_Str());
        }
    }
}

void MeshLoader::ProcessMesh(aiMesh* AssimpMesh, const aiScene* Scene, const XMMATRIX& NodeTransform, vector<Mesh>& OutMeshes, std::vector<PBRTextureNamesPatch>& OutTextureNamesPatches)
{
    Mesh OutMesh;
    PBRTextureNamesPatch OutTextureNamesPatch;

    XMStoreFloat4x4(&OutMesh.Local2WorldMatrix, NodeTransform);
    
    XMMATRIX NormalTransform = XMMatrixTranspose(XMMatrixInverse(nullptr, NodeTransform));

    for (unsigned int I = 0; I < AssimpMesh->mNumVertices; ++I)
    {
        Vertex Vertex;

        // Transform position (w=1, includes translation)
        XMVECTOR Pos = XMVectorSet(
            AssimpMesh->mVertices[I].x,
            AssimpMesh->mVertices[I].y,
            AssimpMesh->mVertices[I].z,
            1.0f
        );
        Pos = XMVector3Transform(Pos, NodeTransform);
        XMStoreFloat3(&Vertex.Position, Pos);

        if (AssimpMesh->HasNormals())
        {
            // Transform normal with inverse-transpose (w=0, rotation only)
            XMVECTOR Normal = XMVectorSet(
                AssimpMesh->mNormals[I].x,
                AssimpMesh->mNormals[I].y,
                AssimpMesh->mNormals[I].z,
                0.0f
            );
            Normal = XMVector3TransformNormal(Normal, NormalTransform);
            Normal = XMVector3Normalize(Normal);
            XMStoreFloat3(&Vertex.Normal, Normal);
        }
        else
        {
            Vertex.Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
        }

        if (AssimpMesh->HasTangentsAndBitangents())
        {
            // Transform tangent with inverse-transpose (w=0, rotation only)
            XMVECTOR Tangent = XMVectorSet(
                AssimpMesh->mTangents[I].x,
                AssimpMesh->mTangents[I].y,
                AssimpMesh->mTangents[I].z,
                0.0f
            );
            Tangent = XMVector3TransformNormal(Tangent, NormalTransform);
            Tangent = XMVector3Normalize(Tangent);
            XMStoreFloat3(&Vertex.Tangent, Tangent);
        }
        else
        {
            Vertex.Tangent = XMFLOAT3(1.0f, 0.0f, 0.0f);
        }

        if (AssimpMesh->mTextureCoords[0])
        {
            Vertex.UV0.x = AssimpMesh->mTextureCoords[0][I].x;
            Vertex.UV0.y = AssimpMesh->mTextureCoords[0][I].y;
        }
        else
        {
            Vertex.UV0 = XMFLOAT2(0.0f, 0.0f);
        }

        OutMesh.Vertices.push_back(Vertex);
    }

    for (unsigned int I = 0; I < AssimpMesh->mNumFaces; ++I)
    {
        aiFace Face = AssimpMesh->mFaces[I];
        
        for (unsigned int J = 0; J < Face.mNumIndices; ++J)
        {
            OutMesh.Indices.push_back(Face.mIndices[J]);
        }
    }
    
    vector<float> Positions;
    Positions.reserve(OutMesh.Vertices.size() * 3);
    for (const auto& Vertex : OutMesh.Vertices)
    {
        Positions.push_back(Vertex.Position.x);
        Positions.push_back(Vertex.Position.y);
        Positions.push_back(Vertex.Position.z);
    }
    
    meshopt_Bounds SphereBounds = meshopt_computeSphereBounds(
        Positions.data(),
        OutMesh.Vertices.size(),
        sizeof(float) * 3,
        nullptr,
        0);
    
    OutMesh.BoundingSphere.x = SphereBounds.center[0];
    OutMesh.BoundingSphere.y = SphereBounds.center[1]; 
    OutMesh.BoundingSphere.z = SphereBounds.center[2];
    OutMesh.BoundingSphere.w = SphereBounds.radius;

    OutMesh.Name = AssimpMesh->mName.C_Str();
    aiMaterial* Material = Scene->mMaterials[AssimpMesh->mMaterialIndex];
    ExtractPBRTextures(Material, OutTextureNamesPatch);
    OutTextureNamesPatches.push_back(OutTextureNamesPatch);
    OutMeshes.push_back(OutMesh);
}

void MeshLoader::ProcessNode(aiNode* Node, const aiScene* Scene, const XMMATRIX& ParentTransform, vector<Mesh>& OutMeshes, std::vector<PBRTextureNamesPatch>& OutTextureNamesPatches)
{
    const XMMATRIX NodeTransform = MathTool::GetInstance().AssimpMatrixToXMMatrix(Node->mTransformation);
    const XMMATRIX WorldTransform = ParentTransform * NodeTransform;
    
    for (unsigned int I = 0; I < Node->mNumMeshes; I++)
    {
        aiMesh* AssimpMesh = Scene->mMeshes[Node->mMeshes[I]];
        ProcessMesh(AssimpMesh, Scene, WorldTransform, OutMeshes, OutTextureNamesPatches);
    }
    
    for (unsigned int I = 0; I < Node->mNumChildren; I++)
    {
        ProcessNode(Node->mChildren[I], Scene, WorldTransform, OutMeshes, OutTextureNamesPatches);
    }
}


ErrorCode MeshLoader::LoadMesh(const string& Path, vector<Mesh>& OutMeshes, std::vector<PBRTextureNamesPatch>& OutTextureNamesPatches)
{
    Assimp::Importer Importer;

    constexpr unsigned int PostProcessFlags = 
        aiProcess_Triangulate |
        aiProcess_MakeLeftHanded |
        aiProcess_FlipWindingOrder |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_FlipUVs;
    
    const aiScene* Scene = Importer.ReadFile(Path, PostProcessFlags);

    if (!Scene || Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !Scene->mRootNode)
    {
        return ErrorCode::MeshDataIncomplete;
    }

    ProcessNode(Scene->mRootNode, Scene, XMMatrixIdentity(), OutMeshes, OutTextureNamesPatches);

    return ErrorCode::OK;
}

ErrorCode MeshLoader::Nanite(const Mesh& Mesh, const std::vector<unsigned int>& TriangleMaterialIDs, std::vector<ClusterData>& OutClusters, std::vector<CLODBound>& OutGroupBounds)
{
    // Validate input: one material ID per triangle
    if (TriangleMaterialIDs.size() != Mesh.Indices.size() / 3)
    {
        return ErrorCode::Failed;
    }

    // Configure Nanite LOD hierarchy generation
    clodConfig Config = clodDefaultConfig(126);

    // Mesh Shader hardware limit: 64 vertices per cluster
    Config.max_vertices = 64;

    // UV soft protection weights
    const float AttributeWeights[2] = { 0.5f, 0.5f };

    // Setup input mesh data
    clodMesh CMesh = {};
    CMesh.indices = Mesh.Indices.data();
    CMesh.index_count = Mesh.Indices.size();
    CMesh.vertex_count = Mesh.Vertices.size();
    CMesh.vertex_positions = &Mesh.Vertices[0].Position.x;
    CMesh.vertex_positions_stride = sizeof(Vertex);
    CMesh.vertex_attributes = &Mesh.Vertices[0].UV0.x;
    CMesh.vertex_attributes_stride = sizeof(Vertex);
    CMesh.attribute_weights = AttributeWeights;
    CMesh.attribute_count = 2;
    CMesh.attribute_protect_mask = 0;
    CMesh.vertex_lock = nullptr;

    // Pass material IDs to enable material-aware simplification
    CMesh.triangle_materials = const_cast<unsigned int*>(TriangleMaterialIDs.data());

    OutClusters.clear();
    OutGroupBounds.clear();

    // Build Nanite cluster hierarchy
    clodBuild(Config, CMesh, [&](clodGroup Group, const clodCluster* Clusters,
                                 size_t ClusterCount) -> int {

        for (size_t I = 0; I < ClusterCount; ++I)
        {
            const clodCluster& Cluster = Clusters[I];
            ClusterData Data;

            // Deduplicate vertices: global indices -> unique vertices + local indices (0-255)
            Data.UniqueVertices.resize(Cluster.vertex_count);
            Data.LocalIndices.resize(Cluster.index_count);

            size_t UniqueCount = clodLocalIndices(
                Data.UniqueVertices.data(),
                Data.LocalIndices.data(),
                Cluster.indices,
                Cluster.index_count
            );

            Data.UniqueVertices.resize(UniqueCount);

            // Extract per-triangle material IDs for this cluster
            size_t TriangleCount = Cluster.index_count / 3;
            Data.TriangleMaterialIDs.resize(TriangleCount);

            if (Cluster.materials)
            {
                for (size_t TriIdx = 0; TriIdx < TriangleCount; ++TriIdx)
                {
                    Data.TriangleMaterialIDs[TriIdx] = Cluster.materials[TriIdx];
                }
            }
            else
            {
                for (size_t TriIdx = 0; TriIdx < TriangleCount; ++TriIdx)
                {
                    Data.TriangleMaterialIDs[TriIdx] = 0;
                }
            }

            // Save Refined value for LOD selection (from meshoptimizer)
            Data.Refined = Cluster.refined;

            // Bounds: center/radius for frustum culling, error for LOD selection
            Data.Bound.Center[0] = Cluster.bounds.center[0];
            Data.Bound.Center[1] = Cluster.bounds.center[1];
            Data.Bound.Center[2] = Cluster.bounds.center[2];
            Data.Bound.Radius = Cluster.bounds.radius;
            Data.Bound.Error = Cluster.bounds.error;

            // GroupId: index to OutGroupBounds for runtime cluster selection
            Data.GroupId = int(OutGroupBounds.size());

            OutClusters.push_back(std::move(Data));
        }

        // Group simplified bounds (monotonic error: parent > child)
        CLODBound GroupBound;
        GroupBound.Center[0] = Group.simplified.center[0];
        GroupBound.Center[1] = Group.simplified.center[1];
        GroupBound.Center[2] = Group.simplified.center[2];
        GroupBound.Radius = Group.simplified.radius;
        GroupBound.Error = Group.simplified.error;
        OutGroupBounds.push_back(GroupBound);

        // Return group index for parent cluster's refined field
        return int(OutGroupBounds.size() - 1);
    });

    return ErrorCode::OK;
}

void MeshLoader::VerifyNaniteHierarchy(const NaniteData& Data, const Mesh& SourceMesh, const std::string& MeshName)
{
    const auto& Clusters = Data.Clusters;
    const auto& GroupBounds = Data.GroupBounds;

    DebugPrintf("\nNanite Verify: %s (%zu clusters, %zu groups)\n", MeshName.c_str(), Clusters.size(), GroupBounds.size());

    if (Clusters.empty())
    {
        DebugPrintf("  ERROR: No clusters found!\n");
        return;
    }

    size_t TotalErrors = 0;

    // Build group -> clusters mapping (used by multiple checks)
    std::map<int, std::vector<size_t>> GroupToClusters;
    for (size_t I = 0; I < Clusters.size(); ++I)
        GroupToClusters[Clusters[I].GroupId].push_back(I);

    // Collect unique materials per cluster (used by multiple checks)
    std::vector<std::set<unsigned int>> ClusterMats(Clusters.size());
    for (size_t I = 0; I < Clusters.size(); ++I)
        for (unsigned int M : Clusters[I].TriangleMaterialIDs)
            ClusterMats[I].insert(M);

    // 1. Data Integrity
    DebugPrintf("  [1] Data Integrity: ");
    size_t IntegrityErrors = 0;
    for (size_t I = 0; I < Clusters.size(); ++I)
    {
        const ClusterData& C = Clusters[I];

        if (C.TriangleMaterialIDs.size() != C.LocalIndices.size() / 3)
        {
            DebugPrintf("\n    Cluster %zu: TriangleMaterialIDs=%zu != TriCount=%zu",
                I, C.TriangleMaterialIDs.size(), C.LocalIndices.size() / 3);
            IntegrityErrors++;
        }
        if (C.GroupId < 0 || C.GroupId >= (int)GroupBounds.size())
        {
            DebugPrintf("\n    Cluster %zu: GroupId=%d out of range [0, %zu)",
                I, C.GroupId, GroupBounds.size());
            IntegrityErrors++;
        }
        if (C.Refined != -1 && (C.Refined < 0 || C.Refined >= (int)GroupBounds.size()))
        {
            DebugPrintf("\n    Cluster %zu: Refined=%d out of range [0, %zu)",
                I, C.Refined, GroupBounds.size());
            IntegrityErrors++;
        }
        for (unsigned int V : C.UniqueVertices)
        {
            if (V >= SourceMesh.Vertices.size())
            {
                DebugPrintf("\n    Cluster %zu: vertex %u >= vertex count %zu",
                    I, V, SourceMesh.Vertices.size());
                IntegrityErrors++;
                break;
            }
        }
    }
    DebugPrintf("%s (%zu)\n", IntegrityErrors == 0 ? "PASS" : "FAIL", IntegrityErrors);
    TotalErrors += IntegrityErrors;

    // 2. DAG Structure
    DebugPrintf("  [2] DAG Structure: ");
    size_t DAGErrors = 0;
    size_t LeafClusters = 0;
    for (size_t I = 0; I < Clusters.size(); ++I)
    {
        if (Clusters[I].Refined == -1)
        {
            LeafClusters++;
            continue;
        }
        if (GroupToClusters.find(Clusters[I].Refined) == GroupToClusters.end())
        {
            DebugPrintf("\n    Cluster %zu: Refined=%d references empty group", I, Clusters[I].Refined);
            DAGErrors++;
        }
    }
    DebugPrintf("%s (%zu) leaf=%zu parent=%zu\n",
        DAGErrors == 0 ? "PASS" : "FAIL", DAGErrors, LeafClusters, Clusters.size() - LeafClusters);
    TotalErrors += DAGErrors;

    // 3. Error Monotonicity
    DebugPrintf("  [3] Error Monotonicity: ");
    size_t MonotonicityErrors = 0;
    for (size_t I = 0; I < Clusters.size(); ++I)
    {
        const ClusterData& C = Clusters[I];
        if (C.Refined == -1) continue;

        int PG = C.GroupId, CG = C.Refined;
        if (PG >= 0 && PG < (int)GroupBounds.size() && CG >= 0 && CG < (int)GroupBounds.size())
        {
            float PE = GroupBounds[PG].Error, CE = GroupBounds[CG].Error;
            if (PE < CE && CE < FLT_MAX && PE < FLT_MAX)
            {
                DebugPrintf("\n    Cluster %zu: parent group[%d] error %.6f < child group[%d] error %.6f",
                    I, PG, PE, CG, CE);
                MonotonicityErrors++;
            }
        }
    }
    DebugPrintf("%s (%zu)\n", MonotonicityErrors == 0 ? "PASS" : "FAIL", MonotonicityErrors);
    TotalErrors += MonotonicityErrors;

    // 4. Material Subset (parent materials must be subset of child materials)
    DebugPrintf("  [4] Material Subset: ");
    size_t SubsetErrors = 0;
    size_t SingleMat = 0, MultiMat = 0, MaxMats = 0;
    for (size_t I = 0; I < Clusters.size(); ++I)
    {
        size_t N = ClusterMats[I].size();
        if (N <= 1) SingleMat++; else MultiMat++;
        if (N > MaxMats) MaxMats = N;

        const ClusterData& Parent = Clusters[I];
        if (Parent.Refined == -1) continue;

        std::set<unsigned int> ChildMats;
        if (GroupToClusters.find(Parent.Refined) != GroupToClusters.end())
            for (size_t ChildIdx : GroupToClusters[Parent.Refined])
                ChildMats.insert(ClusterMats[ChildIdx].begin(), ClusterMats[ChildIdx].end());

        for (unsigned int M : ClusterMats[I])
        {
            if (ChildMats.find(M) == ChildMats.end())
            {
                DebugPrintf("\n    Cluster %zu: material %u not in child group %d", I, M, Parent.Refined);
                SubsetErrors++;
                break;
            }
        }
    }
    DebugPrintf("%s (%zu) single=%zu multi=%zu maxPerCluster=%zu\n",
        SubsetErrors == 0 ? "PASS" : "FAIL", SubsetErrors, SingleMat, MultiMat, MaxMats);
    TotalErrors += SubsetErrors;

    // 5. Terminal Group (must have at least one FLT_MAX group, orphan check)
    DebugPrintf("  [5] Terminal Group: ");
    size_t TerminalErrors = 0;

    // Build reverse map: child group -> parent group (for O(depth) upward traversal)
    std::map<int, int> ChildToParentGroup;
    for (size_t I = 0; I < Clusters.size(); ++I)
        if (Clusters[I].Refined != -1)
            ChildToParentGroup[Clusters[I].Refined] = Clusters[I].GroupId;

    bool HasTerminal = false;
    for (size_t I = 0; I < GroupBounds.size(); ++I)
    {
        float E = GroupBounds[I].Error;
        if (E >= FLT_MAX) HasTerminal = true;
        if (E < 0.0f || E != E)
        {
            DebugPrintf("\n    Group[%zu]: suspicious Error=%.6f", I, E);
            TerminalErrors++;
        }
    }
    if (!HasTerminal)
    {
        DebugPrintf("\n    No terminal group (FLT_MAX) found!");
        TerminalErrors++;
    }

    // Check every cluster can reach a terminal group via upward traversal
    size_t OrphanClusters = 0;
    for (size_t I = 0; I < Clusters.size(); ++I)
    {
        int CurrentGroup = Clusters[I].GroupId;
        std::set<int> Visited;
        bool ReachesTerminal = false;
        while (CurrentGroup >= 0 && CurrentGroup < (int)GroupBounds.size())
        {
            if (Visited.count(CurrentGroup)) break;
            Visited.insert(CurrentGroup);
            if (GroupBounds[CurrentGroup].Error >= FLT_MAX) { ReachesTerminal = true; break; }
            auto It = ChildToParentGroup.find(CurrentGroup);
            if (It == ChildToParentGroup.end()) break;
            CurrentGroup = It->second;
        }
        if (!ReachesTerminal) OrphanClusters++;
    }
    if (OrphanClusters > 0)
        DebugPrintf("\n    Orphan clusters (no path to terminal): %zu", OrphanClusters);

    DebugPrintf("%s (%zu)\n", TerminalErrors == 0 ? "PASS" : "FAIL", TerminalErrors);
    TotalErrors += TerminalErrors;

    // Summary
    DebugPrintf("  RESULT: %s (%zu total errors)\n",
        TotalErrors == 0 ? "ALL PASSED" : "FAILED", TotalErrors);
}