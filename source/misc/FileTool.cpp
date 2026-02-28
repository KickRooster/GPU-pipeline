// Silence C++17 codecvt deprecation warnings
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include "FileTool.h"
#include <locale>
#include <codecvt>
#include <algorithm>
#include <windows.h>
#include <filesystem>

using namespace std;

string FileTool::GetExecutableDirectory() const
{
    char Buffer[MAX_PATH];
    GetModuleFileNameA(nullptr, Buffer, MAX_PATH);

    string ExePath(Buffer);
    size_t LastSlash = ExePath.find_last_of("\\/");

    if (LastSlash != string::npos)
    {
        return ExePath.substr(0, LastSlash);
    }

    return "";
}

string FileTool::FindProjectRoot(const string& StartPath) const
{
    filesystem::path CurrentPath = filesystem::path(StartPath);

    // Search upwards for the directory containing "content" folder
    while (!CurrentPath.empty() && CurrentPath.has_parent_path())
    {
        filesystem::path ContentPath = CurrentPath / "content";

        if (filesystem::exists(ContentPath) && filesystem::is_directory(ContentPath))
        {
            return CurrentPath.string();
        }

        CurrentPath = CurrentPath.parent_path();
    }

    // If not found, return current directory as fallback
    return StartPath;
}

void FileTool::InitializePaths()
{
    string ExeDir = GetExecutableDirectory();
    ProjectRootPath = FindProjectRoot(ExeDir);

    // Initialize all paths relative to project root
    TexturePath = ProjectRootPath + "\\content\\texture";
    ShaderPath = ProjectRootPath + "\\content\\shader";
    MeshPath = ProjectRootPath + "\\content\\mesh";
    ConfigPath = ProjectRootPath + "\\content\\config";

    AmplificationShaderPath = ShaderPath + "\\meshlet_as.hlsl";
    MeshShaderPath = ShaderPath + "\\meshlet_ms.hlsl";
    PixelShaderPath = ShaderPath + "\\meshlet_ps.hlsl";
    ToneMappingCSPath = ShaderPath + "\\tonemapping_cs.hlsl";
    MaterialResolveCSPath = ShaderPath + "\\material_resolve_cs.hlsl";
}

FileTool::FileTool()
{
    InitializePaths();
}

string FileTool::GetProjectRootPath() const
{
    return ProjectRootPath;
}

string FileTool::GetMeshFullPath(const string& RelativePath) const
{
    // If path is already absolute, return as-is
    if (RelativePath.length() > 1 && RelativePath[1] == ':')
    {
        return RelativePath;
    }

    // Otherwise, prepend mesh path
    return MeshPath + "\\" + RelativePath;
}

wstring FileTool::StringToWString(const string& String) const
{
    wstring_convert<codecvt_utf8<wchar_t>> Converter;
    return Converter.from_bytes(String);
}

string FileTool::WStringToString(const wstring& WString) const
{
    wstring_convert<codecvt_utf8<wchar_t>> Converter;
    return Converter.to_bytes(WString);
}

string FileTool::GetTextureFullPath(const string& FilePath) const 
{
    const size_t LastSlashPos = FilePath.find_last_of('\\');
    
    string FileName;
    if (LastSlashPos != string::npos) 
    {
        FileName = FilePath.substr(LastSlashPos + 1);
    }
    else 
    {
        FileName = FilePath;
    }
    
    return TexturePath + "\\" + FileName;
}

string FileTool::GetConfigFullPath(const string& FilePath) const 
{
    const size_t LastSlashPos = FilePath.find_last_of('\\');
    
    string FileName;
    if (LastSlashPos != string::npos) 
    {
        FileName = FilePath.substr(LastSlashPos + 1);
    }
    else 
    {
        FileName = FilePath;
    }
    
    return ConfigPath + "\\" + FileName;
}

string FileTool::GetAmplificationShaderPath() const
{
    return AmplificationShaderPath;
}

string FileTool::GetMeshShaderPath() const
{
    return MeshShaderPath;
}

string FileTool::GetPixelShaderPath() const
{
    return PixelShaderPath;
}


string FileTool::GetToneMappingPath() const
{
    return ToneMappingCSPath;
}

string FileTool::GetMaterialResolveCSPath() const
{
    return MaterialResolveCSPath;
}

string FileTool::GetTextureFolderPath() const
{
    return TexturePath;
}

vector<string> FileTool::GetTextureFiles() const
{
    vector<string> TextureFiles;
    
    string SearchPath = TexturePath + "\\*.*";
    WIN32_FIND_DATA FindData;
    HANDLE FindHandle = FindFirstFile(StringToWString(SearchPath).c_str(), &FindData);
    
    if (FindHandle != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                string FileName = WStringToString(FindData.cFileName);
                if (IsTextureFile(FileName))
                {
                    TextureFiles.push_back(FileName);
                }
            }
        } while (FindNextFile(FindHandle, &FindData));
        
        FindClose(FindHandle);
    }
    
    std::sort(TextureFiles.begin(), TextureFiles.end());
    
    return TextureFiles;
}

bool FileTool::IsTextureFile(const string& FileName) const
{
    string Extension;
    size_t DotPos = FileName.find_last_of('.');
    
    if (DotPos != string::npos)
    {
        Extension = FileName.substr(DotPos);
        transform(Extension.begin(), Extension.end(), Extension.begin(), ::tolower);
    }
    
    return Extension == ".png" || Extension == ".jpg" || Extension == ".jpeg" || 
           Extension == ".tga" || Extension == ".bmp" || Extension == ".hdr";
}