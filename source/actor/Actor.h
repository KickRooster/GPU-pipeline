#pragma once
#include "../misc/Base.h"
#include "../dx12/ConstantBufferProxy.h"
#include <string>

class Actor
{
public:
    std::string Name; 
    Transform Transform;
    Actor() = default;
    virtual void Update(float DeltaTime) = 0;
    virtual ConstantBufferProxy* GetConstantBufferProxy() const { return nullptr; }
    virtual ~Actor() = default;
};
