#pragma once
#include "Base.h"
#include "DesignPatterns.h"
#include <assimp/matrix4x4.h>

class MathTool : public Singleton<MathTool>
{
    friend class Singleton<MathTool>;
    MathTool() = default; 
    ~MathTool() = default;
    
public:
    DirectX::XMFLOAT4X4 Identity4x4()
    {
        static DirectX::XMFLOAT4X4 I(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);

        return I;
    };

    DirectX::XMMATRIX AssimpMatrixToXMMatrix(const aiMatrix4x4& AssimpMatrix)
    {
        return DirectX::XMMATRIX(
            AssimpMatrix.a1, AssimpMatrix.b1, AssimpMatrix.c1, AssimpMatrix.d1,
            AssimpMatrix.a2, AssimpMatrix.b2, AssimpMatrix.c2, AssimpMatrix.d2,
            AssimpMatrix.a3, AssimpMatrix.b3, AssimpMatrix.c3, AssimpMatrix.d3,
            AssimpMatrix.a4, AssimpMatrix.b4, AssimpMatrix.c4, AssimpMatrix.d4
        );
    }

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
    