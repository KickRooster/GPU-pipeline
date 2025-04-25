#include "Triangle.h"
#include "../misc/Math.h"

Triangle::Triangle()
{
    Vertices.push_back({ {0.0f, 0.25f * 1, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} });
    Vertices.push_back({ {0.25f, -0.25f * 1, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f} });
    Vertices.push_back({ {-0.25f, -0.25f * 1, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f} });

    return ;
}