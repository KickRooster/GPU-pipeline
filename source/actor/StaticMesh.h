#pragma once
#include <memory>
#include <vector>
#include "Actor.h"
#include "../asset/Mesh.h"
#include "../asset/Material.h"
#include "../dx12/MeshProxy.h"
#include "../dx12/MaterialProxy.h"

class StaticMesh : public Actor
{
protected:
    FPrimitiveSceneData SceneData;  // GPU Scene data (replaces ConstantBufferInstance)
    std::vector<Vertex> Vertices;

    std::unique_ptr<NaniteData> NaniteDataInstance;
    std::unique_ptr<NaniteClusterProxy> NaniteClusterProxyInstance;
    
    std::unique_ptr<Material> MaterialInstance;
    std::unique_ptr<MaterialProxy> MaterialProxyInstance;

public:
    StaticMesh(
        const DirectX::XMFLOAT4X4* Local2WorldMatrix,
        std::vector<Vertex>&& InVertices,
        std::unique_ptr<NaniteData> InNaniteDataInstance,
        std::unique_ptr<NaniteClusterProxy> InNaniteClusterProxyInstance
        );

    void Update(float DeltaTime, unsigned int FrameIndex) override;
    ConstantBufferProxy* GetConstantBufferProxy() const override;
    const std::vector<Vertex>& GetVertices() const;
    NaniteData* GetNaniteData() const;
    NaniteClusterProxy* GetNaniteClusterProxy() const;
    void SetMaterial(std::unique_ptr<Material> InMaterialInstance, std::unique_ptr<MaterialProxy> InMaterialProxyInstance);
    const Material* GetMaterial() const;
    FPrimitiveSceneData* GetSceneData();
    void ClearData();
    ~StaticMesh() override = default;
};