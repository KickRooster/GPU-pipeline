#include "FileTool.h"
#include <locale>
#include <codecvt>
#include <algorithm>
#include <windows.h>

using namespace std;

std::wstring FileTool::StringToWString(const std::string& String) const
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> Converter;
    return Converter.from_bytes(String);
}

std::string FileTool::WStringToString(const std::wstring& WString) const
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> Converter;
    return Converter.to_bytes(WString);
}

string FileTool::GetTextureFullPath(const std::string& FilePath) const 
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

std::string FileTool::GetTextureFolderPath() const
{
    return TexturePath;
}

std::vector<std::string> FileTool::GetTextureFiles() const
{
    std::vector<std::string> TextureFiles;
    
    std::string SearchPath = TexturePath + "\\*.*";
    WIN32_FIND_DATA FindData;
    HANDLE FindHandle = FindFirstFile(StringToWString(SearchPath).c_str(), &FindData);
    
    if (FindHandle != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                std::string FileName = WStringToString(FindData.cFileName);
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

bool FileTool::IsTextureFile(const std::string& FileName) const
{
    std::string Extension;
    size_t DotPos = FileName.find_last_of('.');
    if (DotPos != std::string::npos)
    {
        Extension = FileName.substr(DotPos);
        std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::tolower);
    }
    
    return Extension == ".png" || Extension == ".jpg" || Extension == ".jpeg" || 
           Extension == ".tga" || Extension == ".bmp" || Extension == ".hdr";
}