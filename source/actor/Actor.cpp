#include "Actor.h"

unsigned Actor::GetSubMeshCount() const
{
    assert(MeshInstances.size() == MeshProxyInstances.size());
    
    return MeshInstances.size();    
}

Mesh* Actor::GetMeshInstance(unsigned int Index) const
{
    if (MeshInstances.size() == 0)
    {
        return nullptr;
    }
    
    return MeshInstances[Index].get();    
}

MeshProxy* Actor::GetMeshProxyInstance(unsigned int Index) const
{
    if (MeshProxyInstances.size() == 0)
    {
        return nullptr;
    }
    
    return MeshProxyInstances[Index].get();    
}

void Actor::Update(float DeltaTime)
{
    XMMATRIX Scale = XMMatrixScaling(Transform.Scale.x, Transform.Scale.y, Transform.Scale.z);
    //  TODO:   rotation process
    //XMMATRIX RotationMatrix =
    XMMATRIX Translation = XMMatrixTranslation(Transform.Position.x, Transform.Position.y, Transform.Position.z);

    TransformationMatrix = Scale * Translation;
}