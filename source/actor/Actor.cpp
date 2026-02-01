#include "Actor.h"

DirectX::XMMATRIX Actor::GetWorldMatrix() const
{
    return TransformationMatrix;
}