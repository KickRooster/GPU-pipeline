#include "SkyLight.h"
#include "../misc/FileTool.h"

using namespace std;

SkyLight::SkyLight()
{
    ConstantBufferInstance = make_unique<SkyLightConstantBuffer>();
    ConstantBufferProxyInstance = make_unique<ConstantBufferProxy>();
}

string SkyLight::GetHDRFilePath() const
{
    return HDRFilePath;
}

void SkyLight::Update(float DeltaTime)
{
    if (ConstantBufferInstance)
    {
        ConstantBufferInstance->IrradianceMapIndex = IrradianceMapProxy->DescriptorIndex;
        ConstantBufferInstance->PrefilteredMapIndex = PrefilteredMapProxy->DescriptorIndex;
        ConstantBufferInstance->BRDFLUTIndex = BRDFLUTProxy->DescriptorIndex;

        if (ConstantBufferProxyInstance->MappedData != nullptr)
        {
            memcpy(
                ConstantBufferProxyInstance->MappedData,
                ConstantBufferInstance.get(),
                MathTool::GetInstance().CalcConstantBufferByteSize(sizeof(SkyLightConstantBuffer)));
        }
    }
}

ConstantBufferProxy* SkyLight::GetConstantBufferProxy() const
{
    return ConstantBufferProxyInstance.get();
}