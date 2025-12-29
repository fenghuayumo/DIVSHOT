#pragma once
#include "core/core.h"
#include "core/ds_log.h"

namespace diverse
{
    class IniFile
    {
    public:
        IniFile(const std::string& filePath);
        ~IniFile() = default;

        void reload();
        void rewrite() const;

        template <typename T>
        T get(const std::string& key);
        template <typename T>
        T get_or_default(const std::string& key, T defaultT);
        template <typename T>
        bool set(const std::string& key, const T& value);
        template <typename T>
        bool add(const std::string& key, const T& value);
        template <typename T>
        bool set_or_add(const std::string& key, const T& value);

        bool remove(const std::string& key);
        void remove_all();
        bool is_key_existing(const std::string& key) const;
        std::vector<std::string> get_formatted_content() const;

        void register_pair(const std::string& key, const std::string& value);
        void register_pair(const std::pair<std::string, std::string>& pair);

        void load();

        std::pair<std::string, std::string> extract_key_and_value(const std::string& attributeLine) const;
        bool is_valid_line(const std::string& attributeLine) const;
        bool string_to_boolean(const std::string& value) const;

    private:
        std::string file_path;
        std::unordered_map<std::string, std::string> data;
    };

    template <typename T>
    inline T IniFile::get(const std::string& key)
    {
        if constexpr (std::is_same<bool, T>::value)
        {
            if (!is_key_existing(key))
                return false;

            return string_to_boolean(data[key]);
        }
        else if constexpr (std::is_same<std::string, T>::value)
        {
            if (!is_key_existing(key))
                return std::string("NULL");

            return data[key];
        }
        else if constexpr (std::is_integral<T>::value)
        {
            if (!is_key_existing(key))
                return static_cast<T>(0);

            return static_cast<T>(std::atoi(data[key].c_str()));
        }
        else if constexpr (std::is_floating_point<T>::value)
        {
            if (!is_key_existing(key))
                return static_cast<T>(0.0f);

            return static_cast<T>(std::atof(data[key].c_str()));
        }
        else
        {
            DS_ASSERT(false, "The given type must be : bool, integral, floating point or string");
            return T();
        }
    }

    template <typename T>
    inline T IniFile::get_or_default(const std::string& key, T defaultT)
    {
        return is_key_existing(key) ? get<T>(key) : defaultT;
    }

    template <typename T>
    inline bool IniFile::set(const std::string& key, const T& value)
    {
        if (is_key_existing(key))
        {
            if constexpr (std::is_same<bool, T>::value)
            {
                data[key] = value ? "true" : "false";
            }
            else if constexpr (std::is_same<std::string, T>::value)
            {
                data[key] = value;
            }
            else if constexpr (std::is_integral<T>::value)
            {
                data[key] = std::to_string(value);
            }
            else if constexpr (std::is_floating_point<T>::value)
            {
                data[key] = std::to_string(value);
            }
            else
            {
                DS_ASSERT(false, "Type not supported");
            }

            return true;
        }

        return false;
    }

    template <typename T>
    inline bool IniFile::set_or_add(const std::string& key, const T& value)
    {
        if (is_key_existing(key))
            return set(key, value);
        else
            return add(key, value);
    }

    template <typename T>
    inline bool IniFile::add(const std::string& key, const T& value)
    {
        if (!is_key_existing(key))
        {
            if constexpr (std::is_same<bool, T>::value)
            {
                register_pair(key, value ? "true" : "false");
            }
            else if constexpr (std::is_same<std::string, T>::value)
            {
                register_pair(key, value);
            }
            else if constexpr (std::is_integral<T>::value)
            {
                register_pair(key, std::to_string(value));
            }
            else if constexpr (std::is_floating_point<T>::value)
            {
                register_pair(key, std::to_string(value));
            }
            else
            {
                DS_ASSERT(false, "Type not supported");
            }

            return true;
        }

        return false;
    }
}
