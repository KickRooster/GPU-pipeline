#pragma once
#include <memory>
#include "../misc/Base.h"
#include "../misc/Math.h"
#include "../dx12/ShapeProxy.h"
#include "../shape/Shape.h"

class Actor
{
protected:
    std::unique_ptr<Shape> ShapeInstance = nullptr;
    std::unique_ptr<ShapeProxy> ShapeProxyInstance = nullptr;
    XMMATRIX TransformationMatrix;
    
public:
    Transform Transform;
    Actor() = default;
    Shape* GetShapeInstance() const ;
    ShapeProxy* GetShapeProxyInstance() const;
    virtual void Update(float DeltaTime);
    virtual ~Actor() = default;
};