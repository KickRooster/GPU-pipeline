#pragma once
#include <string>
#include "Actor.h"

class StaticMeshActor : public Actor
{
public:
    StaticMeshActor(const string& Path);
    void Update(float DeltaTime) override;
    ~StaticMeshActor() override = default;
};