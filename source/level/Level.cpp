#include "Level.h"
#include "../actor/TriangleActor.h"
#include "../actor/CubeActor.h"
#include "../actor/CameraActor.h"

Actor* Level::InstantiateTriagleActor()
{
    unique_ptr<TriangleActor> Actor = make_unique<TriangleActor>();
    Actors.push_back(std::move(Actor));

    return Actors.back().get();
}

Actor* Level::InstantiateCubeActor()
{
    unique_ptr<CubeActor> Actor = make_unique<CubeActor>();
    Actor->Transform.Position.x = 0;
    Actor->Transform.Position.y = 0;
    Actor->Transform.Position.z = 20.f;
    Actor->Transform.Scale.x = 1.0f;
    Actor->Transform.Scale.y = 1.0f;
    Actor->Transform.Scale.z = 1.0f;
    Actor->Transform.Roation.x = 0;
    Actor->Transform.Roation.y = 0;
    Actor->Transform.Roation.z = 0;
    Actors.push_back(std::move(Actor));
    
    return Actors.back().get();
}

Actor* Level::InstantiateCameraActor()
{
    unique_ptr<CameraActor> Actor = make_unique<CameraActor>();
    Actor->FovY = 90.f;
    Actor->AspectRatio = 1.f;
    Actor->NearPlane = 0.1f;
    Actor->FarPlane = 1000.0f;
    Actor->Transform.Position.x = 0;
    Actor->Transform.Position.y = 1.7;
    Actor->Transform.Position.z = -4.0f;
    Actor->LookDirection = XMFLOAT3(0, 0, 1);
    Actor->UpDirection = XMFLOAT3(0, 1, 0);
    Actors.push_back(std::move(Actor));

    return Actors.back().get();
}

void Level::Update(float DeletaTime) const
{
    for (const unique_ptr<Actor>& Actor : Actors)
    {
        Actor->Update(DeletaTime);
    }
}

vector<Actor*> Level::GetActors() const
{
    vector<Actor*> Result;

    for (int I = 0; I < Actors.size(); ++I)
    {
        Result.push_back(Actors[I].get());
    }
    
    return Result;    
}

Level::~Level()
{
    Actors.clear();
}
