#include "Cube.h"

#include <DirectXColors.h>

Cube::Cube()
{
    Vertices.push_back({ XMFLOAT4(-1.0f, -1.0f, -1.0f, 1.0f), XMFLOAT4(Colors::White) });
    Vertices.push_back({ XMFLOAT4(-1.0f, +1.0f, -1.0f, 1.0f), XMFLOAT4(Colors::Coral) });
    Vertices.push_back({ XMFLOAT4(+1.0f, +1.0f, -1.0f, 1.0f), XMFLOAT4(Colors::Red) });
    Vertices.push_back({ XMFLOAT4(+1.0f, -1.0f, -1.0f, 1.0f), XMFLOAT4(Colors::Green) });
    Vertices.push_back({XMFLOAT4(-1.0f, -1.0f, +1.0f, 1.0f), XMFLOAT4(Colors::Blue) });
    Vertices.push_back({XMFLOAT4(-1.0f, +1.0f, +1.0f, 1.0f), XMFLOAT4(Colors::Yellow) });
    Vertices.push_back({XMFLOAT4(+1.0f, +1.0f, +1.0f, 1.0f), XMFLOAT4(Colors::Cyan) });
    Vertices.push_back({XMFLOAT4(+1.0f, -1.0f, +1.0f, 1.0f), XMFLOAT4(Colors::Magenta) });
    
    Indices.push_back(0); Indices.push_back(1); Indices.push_back(2);
    Indices.push_back(0); Indices.push_back(2); Indices.push_back(3);
 
    Indices.push_back(4); Indices.push_back(6); Indices.push_back(5);
    Indices.push_back(4); Indices.push_back(7); Indices.push_back(6);

    Indices.push_back(4); Indices.push_back(5); Indices.push_back(1);
    Indices.push_back(4); Indices.push_back(1); Indices.push_back(0);

    Indices.push_back(3); Indices.push_back(2); Indices.push_back(6);
    Indices.push_back(3); Indices.push_back(6); Indices.push_back(7);

    Indices.push_back(1); Indices.push_back(5); Indices.push_back(6);
    Indices.push_back(1); Indices.push_back(6); Indices.push_back(2);

    Indices.push_back(4); Indices.push_back(0); Indices.push_back(3);
    Indices.push_back(4); Indices.push_back(3); Indices.push_back(7);
}
