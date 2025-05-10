#pragma once
#include "../misc/Base.h"

class Actor
{
public:
    Transform Transform;
    Actor() = default;
    virtual void Update(float DeltaTime) = 0;
    virtual ~Actor() = default;
};