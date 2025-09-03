#pragma once
#include "../misc/DesignPatterns.h"
#include <string>
#include <vector>

class FileTool : public Singleton<FileTool>
{
    const std::string TexturePath = "D:\\GPU-pipeline\\content\\texture";
    const std::string ShaderPath = "D:\\GPU-pipeline\\content\\shader";

    const std::string AmplificationShaderPath = "D:\\GPU-pipeline\\content\\shader\\meshlet_as.hlsl";
    const std::string MeshShaderPath = "D:\\GPU-pipeline\\content\\shader\\meshlet_ms.hlsl";
    const std::string PixelShaderPath = "D:\\GPU-pipeline\\content\\shader\\meshlet_ps.hlsl";
    const std::string ToneMappingCSPath = "D:\\GPU-pipeline\\content\\shader\\tonemapping_cs.hlsl";
    
public:
    std::wstring StringToWString(const std::string& String) const;
    std::string WStringToString(const std::wstring& WString) const;
    std::string GetTextureFullPath(const std::string& FilePath) const;
    std::string GetAmplificationShaderPath() const;
    std::string GetMeshShaderPath() const;
    std::string GetPixelShaderPath() const;
    std::string GetToneMappingPath() const;
    std::string GetTextureFolderPath() const;
    std::vector<std::string> GetTextureFiles() const;
    bool IsTextureFile(const std::string& FileName) const;
};