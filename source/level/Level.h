#pragma once
#include <vector>
#include <memory>
#include "../actor/Actor.h"
#include "../actor/CameraActor.h"
#include "../actor/CubeActor.h"
#include "../misc/DesignPatterns.h"

using namespace std;

class Level : public Singleton<Level>
{
    vector<unique_ptr<Actor>> Actors;

public:
    Actor* InstantiateTriagleActor();
    Actor* InstantiateCubeActor();
    Actor* InstantiateCameraActor();
    void Update(float DeletaTime) const;
    vector<Actor*> GetActors() const;
    ~Level();
};