#include "core/core.h"
#include <fstream>
#include <vector>
#include <iostream>
#include "file_utils.h"
#ifdef DS_PLATFORM_WINDOWS
#include <Windows.h>
#include <commdlg.h>
#else 
#ifdef DS_PLATFORM_MACOS
#include <unistd.h>
#include <libproc.h>
#else
#include <unistd.h>
#include <limits.h>
#endif
#endif

#include <format>

#define DS_LOG_ERROR(...) std::cerr << std::format(__VA_ARGS__)
#define DS_LOG_INFO(...) std::cout << std::format(__VA_ARGS__)

namespace diverse
{
	
	std::vector<std::string> read_text_file(const std::filesystem::path& file_path) {
		std::ifstream file(file_path);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open " + file_path.string());
		}
		std::vector<std::string> lines;
		std::string line;
		while (std::getline(file, line)) {
			if (line.starts_with("#")) {
				continue; // Skip comment lines
			}
			if (!line.empty() && line.back() == '\r') {
				line.pop_back(); // Remove trailing carriage return
			}
			lines.push_back(line);
		}
		file.close();
		if (lines.empty()) {
			throw std::runtime_error("File " + file_path.string() + " is empty or contains no valid lines");
		}
		// Ensure the last line is not empty
		if (lines.back().empty()) {
			lines.pop_back(); // Remove last empty line if it exists
		}
		return lines;
	}

	bool loadText(const std::filesystem::path& path, std::string& text)
	{
		std::ifstream	input(path.string());
		if (!input.good())
		{            
            input.close();
			text = std::string();
			return false;
		}

		std::string line;
		std::string content;
		while (std::getline(input, line))
		{
			content += line;
			content += "\n";
		}
		text = std::move(content);
		return true;
	}

	bool readByteData(const std::string& fileName, std::vector<uint8_t>& data, size_t& dataSize)
	{
		std::ifstream file(fileName, std::ios::binary | std::ios::ate);
		if (file.is_open())
		{
			dataSize = (size_t)file.tellg();
			file.seekg(0, file.beg);
			data.resize(dataSize);
			auto udata = (char*)(data.data());
			file.read(udata, dataSize);
			file.close();
			return true;
		}
		DS_LOG_ERROR("File not found: {}", fileName);
		return false;
	}

	bool	readByteData(const std::string& fileName, uint8_t* dst, size_t size)
	{
		std::ifstream in(fileName, std::ios::binary | std::ios::ate);
		if (in.is_open())
		{
			in.read(reinterpret_cast<char*>(dst), size);
			in.close();
			return true;
		}
		DS_LOG_ERROR("File not found: {}", fileName);
		return false;
	}

	bool writeData(const std::string& fileName, const uint8_t* data, size_t size)
	{
		std::ofstream out(fileName, std::ios::binary | std::ios::ate);
		out.write(reinterpret_cast<const char*>(data), size);
		out.close();
		return true;
	}

	bool write_text(const std::string& path, const std::string& text)
	{
		std::ofstream stream(path, std::ios::binary | std::ios::trunc);

        if(!stream)
        {
            stream.close();
            return false;
        }
		auto buffer = (uint8_t*)&text[0];
		auto buffer_size = (uint32_t)text.size();
        stream.write((char*)buffer, buffer_size);
        stream.close();

        return true;
	}

	bool directoryExists(const std::string& path)
	{
		std::filesystem::path p(path);
		return std::filesystem::exists(p) && std::filesystem::is_directory(p);
	}

	std::string parentDirectory(const std::string& str)
	{
		const std::string::size_type pos = str.find_last_of("/\\");
		// If no separator, return empty path.
		if (pos == std::string::npos) {
			return "";
		}
		// If the separator is not trailing, we are done. 
		if (pos < str.size() - 1) {
			return str.substr(0, pos);
		}
		// Else we have to look for the previous one.
		const std::string::size_type pos1 = str.find_last_of("/\\", pos - 1);
		return str.substr(0, pos1);
	}

	std::string getInstallDirectory()
	{
		char exePath[4095];

#if defined(_WIN32) || defined(_WIN64)
		unsigned int len = GetModuleFileNameA(GetModuleHandleA(0x0), exePath, MAX_PATH);

		std::string installDirectory = parentDirectory(parentDirectory(exePath));
#else
		unsigned int len = 0;

		char result[PATH_MAX];
#ifdef DS_PLATFORM_MACOS
        pid_t pid = getpid();
        size_t c = proc_pidpath(pid, result, sizeof(result));
#else
    	size_t c = readlink("/proc/self/exe", result, sizeof(result) - 1);
#endif
        len = c;
        if( c != -1)
        {
            result[len] = '\0';
//            DS_LOG_INFO("path: {}", result);
        }
        else
            DS_LOG_ERROR("Cant find executable path  ");

		std::string installDirectory = (parentDirectory(parentDirectory(result)));
#ifdef DS_PLATFORM_MACOS
        installDirectory = parentDirectory(parentDirectory(parentDirectory(installDirectory)));
#endif
#endif

		if (len == 0 &&
			!directoryExists(installDirectory + "/bin")) // memory not sufficient or general error occured
		{
			DS_LOG_ERROR("Can't find install folder! Please specify as command-line option using --appPath option!");
		}
		return installDirectory;
	}
    
    std::string getExecutablePath()
    {
        char exePath[4096];

#if defined(_WIN32) || defined(_WIN64)
        unsigned int len = GetModuleFileNameA(GetModuleHandleA(0x0), exePath, MAX_PATH);

        std::string installDirectory = exePath;
#else
        unsigned int len = 0;

        char result[PATH_MAX];
#ifdef DS_PLATFORM_MACOS
        pid_t pid = getpid();
        size_t c = proc_pidpath(pid, result, sizeof(result));
#else
    	size_t c = readlink("/proc/self/exe", result, sizeof(result) - 1);
#endif
        len = c;
        if( c != -1)
        {
            result[len] = '\0';
//            DS_LOG_INFO("path: {}", result);
        }
        else
            DS_LOG_ERROR("Cant find executable path  ");

        std::string installDirectory = result;
#ifdef DS_PLATFORM_MACOS
        installDirectory = installDirectory;
#endif
#endif
        return installDirectory;
    }

	int do_system(const std::string& cmd) {
#ifdef DS_PLATFORM_WINDOWS
		DS_LOG_INFO("> {}", cmd);
	return system(cmd.c_str());
#else
		DS_LOG_INFO("$ {}", cmd);
	return system(cmd.c_str());
#endif
	}


void add_dll_directory(const std::string& path,bool load)
{
	if( !std::filesystem::exists(path))
		return;
#if defined(_WIN32) || defined(_WIN64)
	//convert string path to wstring
	std::wstring wpath(path.begin(), path.end());
	AddDllDirectory(wpath.c_str());
	if(load)
	{
		for (const auto& entry : std::filesystem::directory_iterator(path)) 
		{
			if (entry.is_regular_file() )
			{
				if( entry.path().extension() == ".dll" )
				{
					auto res = LoadLibraryA(entry.path().string().c_str());
					if (!res)  std::cerr << "Failed to load DLL: " << entry.path().string()  << " error:" << GetLastError() << std::endl;
				}
			}
		}
	}
#else
#endif
}

void set_dll_directory(const std::string& path)
{
#if defined(_WIN32) || defined(_WIN64)
	//convert string path to wstring
	std::wstring wpath(path.begin(), path.end());
	std::cout << "path: "<< path <<std::endl;
	SetDllDirectory(wpath.c_str());
	for (const auto& entry : std::filesystem::directory_iterator(path))
	{
		if (entry.is_regular_file())
		{
			if (entry.path().extension() == "dll")
			{
				auto res = LoadLibraryA(entry.path().string().c_str());
				if (!res)  std::cerr << "Failed to load DLL: " << GetLastError() << std::endl;
			}
		}
	}
#else
#endif
}
void add_env(const std::string& name, const std::string& value)
{
#if defined(_WIN32) || defined(_WIN64)
	if (_putenv_s(name.c_str(), value.c_str()) != 0)
	{
		std::cerr << "Failed to set environment variable: " << name << std::endl;
	}
#else
	// if ( setenv(name.c_str(), value.c_str(), 1) !=0)
	// {
	// 	std::cerr << "Failed to set environment variable: " << name << std::endl;
	// }
#endif
}

void add_env(const std::string& name)
{
#if defined(_WIN32) || defined(_WIN64)
	if (_putenv(name.c_str()) != 0)
	{
		std::cerr << "Failed to set environment variable: " << name << std::endl;
	}
#else
	// if (setenv(name.c_str()))
	// {
	// 	std::cerr << "Failed to set environment variable: " << name << std::endl;
	// }
#endif
}
} // namespace diverse
