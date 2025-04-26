#include "CubeActor.h"
#include "../shape/Cube.h"

CubeActor::CubeActor()
{
    ShapeInstance = make_unique<Cube>();
    ShapeProxyInstance = make_unique<ShapeProxy>();
}

void CubeActor::Update(float DeltaTime)
{
    
}
