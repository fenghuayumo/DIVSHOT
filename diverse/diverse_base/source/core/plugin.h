#pragma once
#if DS_PLATFORM_WINDOWS
#include <Windows.h>
#elif DS_PLATFORM_LINUX || DS_PLATFORM_MCAOS
#include <dlfcn.h>
#endif
#include "core.h"
#include "utility/singleton.h"
#include <string>
#include <filesystem>
#include <map>
#include <memory>
namespace
{
	typedef void *(*createInstanceFunc)();
	typedef const char *(*getDescriptionFunc)();
}

using Path = std::filesystem::path;
using ObjHandle = void*;
namespace diverse
{
	class DS_EXPORT Plugin
	{
	public:
		Plugin() = default;
		/// Load a plugin from the supplied path
		Plugin(const std::string &shortName, const Path &path);

		/// Virtual destructor
		virtual ~Plugin();

		/// Return an instance of the class implemented by this plugin
		ObjHandle create_instance() const;

		/// Return a description of this plugin
		std::string get_description() const;

		/// Return the path of this plugin
		const Path& path() const;

		/// Return a short name of this plugin
		const std::string& short_name() const;
		/// Resolve the given symbol and return a pointer
		void* get_symbol(const std::string& sym);
	protected:
		/// Check whether a certain symbol is provided by the plugin
		bool has_symbol(const std::string &sym) const;

		createInstanceFunc	m_createInstance;
		getDescriptionFunc	m_DescriptInstance;
		const std::string	m_shortName;
		const Path			m_path;
#if defined(DS_PLATFORM_WINDOWS)
		HMODULE m_handle;
#else
		void *m_handle;
#endif
	private:

		auto get_last_error_as_string() const->std::string;
	};

	class DS_EXPORT PluginManager : public ThreadSafeSingleton<PluginManager>
	{
	public:
		 ObjHandle	create_object(const std::string& name);

		 std::vector<std::string>	get_loaded_plugins();

		 bool	ensure_plugin_loaded(const std::string &name);
		 Plugin* get_plugin(const std::string& name);
	private:
		std::map<std::string, std::unique_ptr<Plugin>>	m_plugins;
	};

	extern "C" {
		DS_EXPORT PluginManager&	get_plugin_manager();
	}

#define DS_IMPL_PLUGIN(ClassName,Description)\
	extern "C" { \
		void DS_EXPORT* create_instance() {\
			return new ClassName();     \
		}\
		const char DS_EXPORT* get_description(){\
			return Description;\
		}\
	}
}