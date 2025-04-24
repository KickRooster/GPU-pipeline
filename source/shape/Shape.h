#pragma once
#include <array>

#include "../base/Math.h"
#include <vector>

using namespace std;

class Shape
{
protected:
    vector<Vertex4> Vertices;
    vector<unsigned int> Indices;

public:
    std::array<Vertex, 8> vertices;
    std::array<uint16_t, 36> indices;
    
public:
    Shape() = default;
    
    const vector<Vertex4>& GetVertices() const
    {
        return Vertices;
    }

    const vector<unsigned int>& GetIndices() const
    {
        return Indices;
    }
    
    virtual ~Shape() = default;
};