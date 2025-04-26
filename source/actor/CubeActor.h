#pragma once
#include "Actor.h"

class CubeActor : public Actor
{
public:
    CubeActor();
    void Update(float DeltaTime) override;
    ~CubeActor() override = default;
};