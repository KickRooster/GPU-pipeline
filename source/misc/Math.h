#pragma once
#include <DirectXMath.h>
#include "DesignPatterns.h"

using namespace DirectX;

struct Vertex
{
    XMFLOAT4 Pos;
    XMFLOAT4 Color;
};

struct Transform
{
    XMFLOAT4 Rotation;
    XMFLOAT3 Position;
    XMFLOAT3 Scale;
};

class MathTool : public Singleton<MathTool>
{
    friend class Singleton<MathTool>;
    MathTool() = default; 
    ~MathTool() = default;
    
public:
    XMFLOAT4X4 Identity4x4()
    {
        static XMFLOAT4X4 I(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);

        return I;
    };

    unsigned int CalcConstantBufferByteSize(unsigned int ByteSize) const
    {
        // Constant buffers must be a multiple of the minimum hardware
        // allocation size (usually 256 bytes).  So round up to nearest
        // multiple of 256.  We do this by adding 255 and then masking off
        // the lower 2 bytes which store all bits < 256.
        // Example: Suppose byteSize = 300.
        // (300 + 255) & ~255
        // 555 & ~255
        // 0x022B & ~0x00ff
        // 0x022B & 0xff00
        // 0x0200
        // 512
        
        return (ByteSize + 255) & ~255;
    }
};
    