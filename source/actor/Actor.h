#pragma once
#include "../misc/Base.h"
#include <string>

class Actor
{
public:
    std::string Name; 
    Transform Transform;
    Actor() = default;
    virtual void Update(float DeltaTime) = 0;
    virtual ~Actor() = default;
};