#include "TriangleActor.h"
#include "../shape/Triangle.h"

TriangleActor::TriangleActor()
{
    ShapeInstance = make_unique<Triangle>();  
}