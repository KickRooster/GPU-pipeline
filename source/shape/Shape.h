#pragma once
#include <array>

#include "../misc/Math.h"
#include <vector>

using namespace std;

class Shape
{
protected:
    vector<Vertex> Vertices;
    vector<unsigned int> Indices;

public:
    Shape() = default;
    
    const vector<Vertex>& GetVertices() const
    {
        return Vertices;
    }

    const vector<unsigned int>& GetIndices() const
    {
        return Indices;
    }
    
    virtual ~Shape() = default;
};