#include "Cube.h"

#include <DirectXColors.h>

Cube::Cube()
{
    vertices =
        {
        Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
        Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
        Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
        Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
        Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
        Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
        Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
        Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
    };

    indices =
    {
        // front face
        0, 1, 2,
        0, 2, 3,

        // back face
        4, 6, 5,
        4, 7, 6,

        // left face
        4, 5, 1,
        4, 1, 0,

        // right face
        3, 2, 6,
        3, 6, 7,

        // top face
        1, 5, 6,
        1, 6, 2,

        // bottom face
        4, 0, 3,
        4, 3, 7
    };
    
    return;
    Vertices.clear();
    Indices.clear();
    
    Vertices.push_back({ {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} });
    Vertices.push_back({ {1.0f, 1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} });
    Vertices.push_back({ {1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} });
    Vertices.push_back({ {1.0f, -1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} });

    Vertices.push_back({ {-1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f} });
    Vertices.push_back({ {-1.0f, 1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f} });
    Vertices.push_back({ {-1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f} });
    Vertices.push_back({ {-1.0f, -1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f} });
    
    Indices.push_back(0); Indices.push_back(1); Indices.push_back(2);
    Indices.push_back(0); Indices.push_back(2); Indices.push_back(3);

    Indices.push_back(4); Indices.push_back(7); Indices.push_back(6);
    Indices.push_back(4); Indices.push_back(6); Indices.push_back(5);

    Indices.push_back(0); Indices.push_back(4); Indices.push_back(5);
    Indices.push_back(0); Indices.push_back(5); Indices.push_back(1);

    Indices.push_back(2); Indices.push_back(6); Indices.push_back(7);
    Indices.push_back(2); Indices.push_back(7); Indices.push_back(3);

    Indices.push_back(0); Indices.push_back(3); Indices.push_back(7);
    Indices.push_back(0); Indices.push_back(7); Indices.push_back(4);

    Indices.push_back(1); Indices.push_back(5); Indices.push_back(6);
    Indices.push_back(1); Indices.push_back(6); Indices.push_back(2);
}
