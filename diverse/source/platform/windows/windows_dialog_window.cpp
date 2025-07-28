#ifdef DS_PLATFORM_WINDOWS
#include <Windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include "engine/dialog_window.h"
#include <format>
namespace diverse
{
	std::pair<std::string,int> FileDialogs::openFile(const std::vector<const char*>& filters,const std::vector<const char*>& desc)
	{
		if( filters.empty())
		{
			//open filbrwoser dialog for direcrory
			BROWSEINFOA bi = { 0 };
			bi.lpszTitle = "Select a folder";
			bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
			LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
			if (pidl != 0)
			{
				// get the name of the folder
				CHAR path[MAX_PATH] = {0};
				SHGetPathFromIDListA(pidl, path);
				// free memory used
				IMalloc* imalloc = 0;
				if (SUCCEEDED(SHGetMalloc(&imalloc)))
				{
					imalloc->Free(pidl);
					imalloc->Release();
				}
				return {path,0};
			}
			return {};
		}
		char format_filter[256] = {0};// = "(*.ply)\0*.ply\0(*.splat)\0*.splat\0(*.compressply)\0*.compressply";
		for (auto i = 0; i< filters.size();i++)
		{
			auto cfilter = filters[i];
			if(i < desc.size())
				strcat(format_filter, desc[i]);
			strcat(format_filter, "(*.");
			strcat(format_filter, cfilter);
			strcat(format_filter, ")|*.");
			strcat(format_filter, cfilter);
			strcat(format_filter, "|");
		}
		int len = strlen(format_filter);
		for (auto i = 0; i < len; i++)
		{
			if (format_filter[i] == '|')
			{
				format_filter[i] = '\0';
			}
		}
		OPENFILENAMEA ofn;
		CHAR szFile[260] = { 0 };
		CHAR currentDir[256] = { 0 };
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = 0;//glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		if (GetCurrentDirectoryA(256, currentDir))
			ofn.lpstrInitialDir = currentDir;
		ofn.lpstrFilter = format_filter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
		ofn.lpstrDefExt = strchr(format_filter, '\0') + 1;
		if (GetOpenFileNameA(&ofn) == TRUE)
			return {ofn.lpstrFile,ofn.nFilterIndex};
		return {};

	}

	std::string FileDialogs::saveFile(const std::vector<const char*>& filters,const std::vector<const char*>& desc)
	{
        // (*.dvs)\0*.dvs\0
		char format_filter[256] = {0};// = "(*.ply)\0*.ply\0(*.splat)\0*.splat\0(*.compressply)\0*.compressply";
		for (auto i = 0; i< filters.size();i++)
		{
			auto cfilter = filters[i];
			if(i < desc.size())
				strcat(format_filter, desc[i]);
			strcat(format_filter, "(*.");
			strcat(format_filter, cfilter);
			strcat(format_filter, ")|*.");
			strcat(format_filter, cfilter);
			strcat(format_filter, "|");
		}
		int len = strlen(format_filter);
		for (auto i = 0; i < len; i++)
		{
			if (format_filter[i] == '|')
			{
				format_filter[i] = '\0';
			}
		}
		OPENFILENAMEA ofn;
		CHAR szFile[260] = { 0 };
		CHAR currentDir[256] = { 0 };
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = 0;//glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		if (GetCurrentDirectoryA(256, currentDir))
			ofn.lpstrInitialDir = currentDir;
		ofn.lpstrFilter = format_filter;
		//ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

		// Sets the default extension by extracting it from the filter
		ofn.lpstrDefExt = strchr(format_filter, '\0') + 1;

		if (GetSaveFileNameA(&ofn) == TRUE)
			return ofn.lpstrFile;
		return std::string();

	}

	void messageBox(const char* title, const char* message)
	{
		//MessageBox
		MessageBoxA(NULL, message, "", MB_OK);
	}
}
#endif