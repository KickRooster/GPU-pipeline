#include "SkyLight.h"
#include "../misc/FileTool.h"

using namespace std;

SkyLight::SkyLight()
{
    ConstantBufferInstance = make_unique<SkyLightConstantBuffer>();
    ConstantBufferProxyInstance = make_unique<ConstantBufferProxy>();

    // Initialize Transform with default values
    Transform.Position = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    Transform.Rotation = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);  // Identity quaternion
    Transform.Scale = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
}

string SkyLight::GetHDRFilePath() const
{
    return HDRFilePath;
}

void SkyLight::Update(float DeltaTime, unsigned int FrameIndex)
{
    const DirectX::XMMATRIX Scale = DirectX::XMMatrixScaling(Transform.Scale.x, Transform.Scale.y, Transform.Scale.z);
    const DirectX::XMVECTOR Quaternion = DirectX::XMLoadFloat4(&Transform.Rotation);
    const DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationQuaternion(Quaternion);

    const DirectX::XMMATRIX Translation = DirectX::XMMatrixTranslation(Transform.Position.x, Transform.Position.y, Transform.Position.z);

    TransformationMatrix = Scale * Rotation * Translation;

    if (ConstantBufferInstance)
    {
        ConstantBufferInstance->IrradianceMapIndex = IrradianceMapProxy->DescriptorIndex;
        ConstantBufferInstance->PrefilteredMapIndex = PrefilteredMapProxy->DescriptorIndex;
        ConstantBufferInstance->BRDFLUTIndex = BRDFLUTProxy->DescriptorIndex;

        if (ConstantBufferProxyInstance->MappedData[FrameIndex] != nullptr)
        {
            memcpy(
                ConstantBufferProxyInstance->MappedData[FrameIndex],
                ConstantBufferInstance.get(),
                MathTool::GetInstance().CalcConstantBufferByteSize(sizeof(SkyLightConstantBuffer)));
        }
    }
}

ConstantBufferProxy* SkyLight::GetConstantBufferProxy() const
{
    return ConstantBufferProxyInstance.get();
}