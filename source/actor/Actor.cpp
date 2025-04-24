#include "Actor.h"

Shape* Actor::GetShapeInstance() const
{
    return ShapeInstance.get();    
}

ShapeProxy* Actor::GetShapeProxyInstance() const
{
    return ShapeProxyInstance.get();    
}

void Actor::Update(float DeltaTime)
{
    XMMATRIX Scale = XMMatrixScaling(Transform.Scale.x, Transform.Scale.y, Transform.Scale.z);
    //  TODO:   rotation process
    //XMMATRIX RotationMatrix =
    XMMATRIX Translation = XMMatrixTranslation(Transform.Position.x, Transform.Position.y, Transform.Position.z);

    TransformationMatrix = Scale * Translation;
}