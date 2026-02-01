#pragma once
#include "../misc/DesignPatterns.h"
#include <string>
#include <vector>

class FileTool : public Singleton<FileTool>
{
    std::string ProjectRootPath;
    std::string TexturePath;
    std::string ShaderPath;
    std::string MeshPath;
    std::string ConfigPath;

    std::string AmplificationShaderPath;
    std::string MeshShaderPath;
    std::string PixelShaderPath;
    std::string ToneMappingCSPath;

private:
    std::string GetExecutableDirectory() const;
    std::string FindProjectRoot(const std::string& StartPath) const;
    void InitializePaths();

public:
    FileTool();

    std::string GetProjectRootPath() const;
    std::string GetMeshFullPath(const std::string& RelativePath) const;
    std::wstring StringToWString(const std::string& String) const;
    std::string WStringToString(const std::wstring& WString) const;
    std::string GetTextureFullPath(const std::string& FilePath) const;
    std::string GetConfigFullPath(const std::string& FilePath) const;
    std::string GetAmplificationShaderPath() const;
    std::string GetMeshShaderPath() const;
    std::string GetPixelShaderPath() const;
    std::string GetToneMappingPath() const;
    std::string GetTextureFolderPath() const;
    std::vector<std::string> GetTextureFiles() const;
    bool IsTextureFile(const std::string& FileName) const;
};