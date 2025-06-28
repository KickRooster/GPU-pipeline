#pragma once
#include "../misc/DesignPatterns.h"
#include <string>

class FileTool : public Singleton<FileTool>
{
    const std::string TexturePath = "D:\\GPU-pipeline\\content\\texture";
    
public:
    std::string GetTextureFullPath(const std::string& FilePath);
};
