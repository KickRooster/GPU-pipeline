#include "FileTool.h"

using namespace std;

string FileTool::GetTextureFullPath(const std::string& FilePath)
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
