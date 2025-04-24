#pragma once
#include <memory>
#include "../base/Base.h"
#include "../base/Math.h"
#include "../dx12/ShapeProxy.h"
#include "../shape/Shape.h"

class Actor
{
protected:
    std::unique_ptr<Shape> ShapeInstance;
    std::unique_ptr<ShapeProxy> ShapeProxyInstance;
    XMMATRIX TransformationMatrix;
public:
    Transform Transform;
    Actor() = default;
    Shape* GetShapeInstance() const ;
    ShapeProxy* GetShapeProxyInstance() const;
    virtual void Update(float DeltaTime);
    virtual ~Actor() = default;
};