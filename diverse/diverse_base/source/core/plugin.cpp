#include "plugin.h"
#include <iostream>
#include <format>
#if DS_PLATFORM_MACOS
#include <dlfcn.h>
#endif

#define DS_LOG_ERROR(...) std::cerr << std::format(__VA_ARGS__) << std::endl
#define DS_LOG_INFO(...) std::cout << std::format(__VA_ARGS__) << std::endl
namespace diverse
{
	ObjHandle PluginManager::create_object(const std::string& name)
	{
		ensure_plugin_loaded(name);
		return m_plugins[name]->create_instance();
	}

	std::vector<std::string> PluginManager::get_loaded_plugins() 
	{
		std::vector<std::string> list;
		for (auto it = m_plugins.begin();it != m_plugins.end(); ++it) {
			list.push_back((*it).first);
		}
		return list;
	}

	Plugin* PluginManager::get_plugin(const std::string& name)
	{
		if (m_plugins.find(name) != m_plugins.end()) {
			return m_plugins[name].get();
		}
		return nullptr;
	}

	bool PluginManager::ensure_plugin_loaded(const std::string & name)
	{
		/* Plugin already loaded? */
		if (m_plugins[name])
			return true;
		// Load plugin
#if DS_PLATFORM_WINDOWS
		const auto parent = Path(name).parent_path().string();
		SetDllDirectoryA(parent.c_str());
#endif
		auto path = Path(name);
		auto shortName =  path.filename().string();
		try {
			auto plugin = std::make_unique<Plugin>(shortName,path);
			if (!plugin)
			{
				DS_LOG_WARN("Failed to load library: {}", name);
			}
#if DS_PLATFORM_WINDOWS
			SetDllDirectoryA(nullptr);
#endif
			m_plugins[name] = std::move(plugin);
			DS_LOG_INFO("Successfully loaded plugin {}", name);
		}
		catch (...) {
			DS_LOG_WARN("create plugin failed");
			return false;
		}
		return true;
	}

	Plugin::Plugin(const std::string & shortName,const Path & path)
		:m_shortName(shortName),m_path(path)
	{
#if DS_PLATFORM_WINDOWS
		const auto p = path.string() + ".dll";
#elif DS_PLATFORM_LINUX
		const auto p =  (path.parent_path() / ("lib" +  shortName + ".so") ).string();
#elif DS_PLATFORM_MACOS
		const auto p =  (path.parent_path() / ("lib" +  shortName + ".dylib") ).string();
#endif

#if DS_PLATFORM_WINDOWS
		m_handle = LoadLibraryA(p.c_str());
		if (!m_handle)
		{
			DS_LOG_ERROR("Failed to load library or its dependencies : {}", p);
			auto error = get_last_error_as_string();
			DS_LOG_ERROR("error : {}",error);
			throw std::runtime_error("Failed to load library");
		}
#elif DS_PLATFORM_LINUX || DS_PLATFORM_MACOS
		m_handle = dlopen(p.c_str(), RTLD_LAZY | RTLD_LOCAL);
		if (!m_handle)
		{
			DS_LOG_ERROR("Failed to load library or its dependencies : {}", p );
			DS_LOG_ERROR(dlerror() );
		}
#endif
		try {
			m_DescriptInstance = (getDescriptionFunc)get_symbol("get_description");
	    }
		catch (...) {
#if defined(DS_PLATFORM_WINDOWS)
			FreeLibrary(m_handle);
#else
			dlclose(m_handle);
#endif
			throw std::runtime_error("can't found plugin description");
		}

		m_createInstance = nullptr;

		m_createInstance = (createInstanceFunc)get_symbol("create_instance");
	}

	Plugin::~Plugin()
	{
#if DS_PLATFORM_WINDOWS
		if (!FreeLibrary(m_handle))
		{
			DS_LOG_ERROR( "Failed to free library" );
			DS_LOG_ERROR("error : {}", get_last_error_as_string());
		}
#elif DS_PLATFORM_LINUX || DS_PLATFORM_MACOS
		if (dlclose(m_handle) != 0)
		{
			DS_LOG_ERROR( "Failed to free library" );
			DS_LOG_ERROR( dlerror() );
		}
#endif
	}

	ObjHandle  Plugin::create_instance() const
	{
		auto ptr = m_createInstance();
		return ptr;
	}

	std::string Plugin::get_description() const
	{
		return m_DescriptInstance();
	}

	const Path & Plugin::path() const
	{
		// TODO: insert return statement here
		return m_path;
	}

	const std::string & Plugin::short_name() const
	{
		// TODO: insert return statement here
		return m_shortName;
	}

	void * Plugin::get_symbol(const std::string & sym)
	{
#if defined(DS_PLATFORM_WINDOWS)
		void *data = GetProcAddress(m_handle, sym.c_str());
		if (!data) {
			DS_LOG_ERROR("Could not resolve symbol {} in {} : {}", sym, m_path.string(), get_last_error_as_string());
		}
#else
		void *data = dlsym(m_handle, sym.c_str());
		if (!data) {
			DS_LOG_ERROR("Could not resolve symbol {} in {} : {}", sym, m_path.string(), get_last_error_as_string());
		}
#endif
		return data;
	}

	bool Plugin::has_symbol(const std::string & sym) const
	{
#if defined(DS_PLATFORM_WINDOWS)
		void *ptr = GetProcAddress(m_handle, sym.c_str());
#else
		void *ptr = dlsym(m_handle, sym.c_str());
#endif
		return ptr != nullptr;
	}

	auto Plugin::get_last_error_as_string() const -> std::string
	{
#if defined(DS_PLATFORM_WINDOWS)
		DWORD error = GetLastError();
		if (error == 0)
		{
			return std::string();
		}

		LPSTR buffer = nullptr;
		size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buffer, 0, NULL);
		std::string message(buffer, size);
		LocalFree(buffer);
		return message;
#else
        return std::string(dlerror());
#endif
	}

	DS_EXPORT PluginManager & get_plugin_manager()
	{
		return PluginManager::get();
	}

}