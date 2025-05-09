#pragma once
#include <memory>
#include "../misc/Base.h"
#include "../misc/Math.h"
#include "../dx12/MeshProxy.h"
#include "../mesh/Mesh.h"

using namespace std;

class Actor
{
protected:
    vector<unique_ptr<Mesh>> MeshInstances;
    vector<unique_ptr<MeshProxy>> MeshProxyInstances;
    XMMATRIX TransformationMatrix;
    
public:
    Transform Transform;
    Actor() = default;
    unsigned int GetSubMeshCount() const;
    Mesh* GetMeshInstance(unsigned int Index) const ;
    MeshProxy* GetMeshProxyInstance(unsigned int Index) const;
    virtual void Update(float DeltaTime);
    virtual ~Actor() = default;
};