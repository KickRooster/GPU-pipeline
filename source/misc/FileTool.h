#pragma once
#include "../misc/DesignPatterns.h"
#include <string>

class FileTool : public Singleton<FileTool>
{
    const std::string TexturePath = "D:\\GPU-pipeline\\content\\texture";
    const std::string ShaderPath = "D:\\GPU-pipeline\\content\\shader";

    const std::wstring AmplificationShaderPath = L"D:\\GPU-pipeline\\content\\shader\\meshlet_as.hlsl";
    const std::wstring MeshShaderPath = L"D:\\GPU-pipeline\\content\\shader\\meshlet_ms.hlsl";
    const std::wstring PixelShaderPath = L"D:\\GPU-pipeline\\content\\shader\\meshlet_ps.hlsl";
    
public:
    std::string GetTextureFullPath(const std::string& FilePath);
    std::wstring GetAmplificationShaderPath() const;
    std::wstring GetMeshShaderPath() const;
    std::wstring GetPixelShaderPath() const;
};
