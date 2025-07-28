#pragma once
#include <string>
#include <vector>
#include <unordered_map>
namespace diverse
{
    class FileDialogs
	{
	public:
		// These return empty strings if cancelled
		static std::pair<std::string,int> openFile(const std::vector<const char*>& filter,const std::vector<const char*>& desc={});
		static std::string saveFile(const std::vector<const char*>& filter,const std::vector<const char*>& desc={});
	};
    enum class MessageBoxInfoType
	{
		Info,
		Warn,
		Error
	};
    void messageBox(const char* title, const char* message);
}
