#pragma once
#include <vector>
#include <memory>
#include <string>

#include "../actor/Actor.h"
#include "../misc/DesignPatterns.h"

using namespace std;

class Level : public Singleton<Level>
{
    vector<unique_ptr<Actor>> Actors;

public:
    Actor* InstantiateStaticMeshActor(const string& Path);
    Actor* InstantiateCameraActor();
    void Update(float DeletaTime) const;
    vector<Actor*> GetActors() const;
    ~Level();
};