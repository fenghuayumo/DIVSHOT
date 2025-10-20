#pragma once
#include <string>
#include <vector>
#include "core/string.h"

#ifdef DS_PLATFORM_ANDROID
template <typename T>
std::string to_string(const T& n)
{
    std::ostringstream stm;
    stm << n;
    return stm.str();
}
#endif
#ifdef DS_PLATFORM_WINDOWS
#include <Windows.h>
#endif
namespace diverse
{
    namespace stringutility
    {
        template <typename T>
        static std::string to_string(const T& input)
        {
#ifdef DS_PLATFORM_ANDROID
            return to_string(input);
#else
            return std::to_string(input);
#endif
        }

        std::string get_file_extension(const std::string& FileName);
        std::string get_file_name(const std::string& FilePath);
        std::string get_file_location(const std::string& FilePath);

        std::string remove_name(const std::string& FilePath);
        std::string remove_file_extension(const std::string& FileName);
        std::string to_lower(const std::string& text);

        std::string& back_slashes_2_slashes(std::string& string);
        std::string& slashes_2_back_slashes(std::string& string);
        std::string& remove_spaces(std::string& string);
        std::string& remove_character(std::string& string, const char character);

        std::string demangle(const std::string& string);

        bool is_hidden_file(const std::string& path);
        std::vector<std::string> DS_EXPORT split_string(const std::string& string, const std::string& delimiters);
        std::vector<std::string> DS_EXPORT split_string(const std::string& string, const char delimiter);
        std::vector<std::string> DS_EXPORT tokenize(const std::string& string);
        std::vector<std::string> get_lines(const std::string& string);

        const char* find_token(const char* str, const std::string& token);
        const char* find_token(const std::string& string, const std::string& token);
        int32_t find_string_position(const std::string& string, const std::string& search, uint32_t offset = 0);
        std::string string_range(const std::string& string, uint32_t start, uint32_t length);
        std::string remove_string_range(const std::string& string, uint32_t start, uint32_t length);

        std::string get_block(const char* str, const char** outPosition = nullptr);
        std::string get_block(const std::string& string, uint32_t offset = 0);

        std::string get_statement(const char* str, const char** outPosition = nullptr);

        bool string_contains(const std::string& string, const std::string& chars);
        bool starts_with(const std::string& string, const std::string& start);
        int32_t next_int(const std::string& string);

        bool string_equals(const std::string& string1, const std::string& string2);
        std::string string_replace(std::string str, char ch1, char ch2);
        std::string string_replace(std::string str, char ch);

        std::string byte_2_string(uint64_t bytes);

        enum PathType : uint8_t
        {
            Relative,
            DriveAbsolute,
            RootAbsolute,
            Count
        };

        String8 str8_skip_whitespace(String8 str);
        String8 str8_chop_whitespace(String8 str);
        String8 str8_skip_chop_whitespace(String8 str);
        String8 str8_skip_chop_newlines(String8 str);

        String8 str8_path_chop_last_period(String8 str);
        String8 str8_path_skip_last_slash(String8 str);
        String8 str8Path_chop_last_slash(String8 str);
        String8 str8_path_skip_last_period(String8 str);
        String8 str8_path_chop_past_last_slash(String8 str);

        PathType path_type_from_str8(String8 path);
        String8List path_parts_from_str8(Arena* arena, String8 path);
        String8List absolute_path_parts_from_source_parts_type(Arena* arena, String8 source, String8List parts, PathType type);

        String8List dot_resolved_path_parts_from_parts(Arena* arena, String8List parts);
        String8 normalized_path_from_str8(Arena* arena, String8 source, String8 path);
        String8 get_file_name(String8 str, bool directory = false);

        uint64_t basic_hash_from_string(String8 string);
    }


    inline	std::wstring string_convert(const std::string& from)
    {
        std::wstring to;
#ifdef _WIN32
        int num = MultiByteToWideChar(CP_UTF8, 0, from.c_str(), -1, NULL, 0);
        if (num > 0)
        {
            to.resize(size_t(num) - 1);
            MultiByteToWideChar(CP_UTF8, 0, from.c_str(), -1, &to[0], num);
        }
#else
        to = std::wstring(from.begin(), from.end()); // TODO
#endif // _WIN32
        return to;
    }

    inline std::string string_convert(const std::wstring& from)
    {
        std::string to;
#ifdef _WIN32
        int num = WideCharToMultiByte(CP_UTF8, 0, from.c_str(), -1, NULL, 0, NULL, NULL);
        if (num > 0)
        {
            to.resize(size_t(num) - 1);
            WideCharToMultiByte(CP_UTF8, 0, from.c_str(), -1, &to[0], num, NULL, NULL);
        }
#else
        to = std::string(from.begin(), from.end()); // TODO
#endif // _WIN32
        return to;
    }

    inline int string_convert(const char* from, wchar_t* to)
    {
#ifdef _WIN32
        int num = MultiByteToWideChar(CP_UTF8, 0, from, -1, NULL, 0);
        if (num > 0)
        {
            MultiByteToWideChar(CP_UTF8, 0, from, -1, &to[0], num);
        }
#else
        std::string sfrom = from;
        std::wstring wto = std::wstring(sfrom.begin(), sfrom.end());
        std::wmemcpy(to, wto.c_str(), wto.length());
        int num = wto.length();
#endif // _WIN32
        return num;
    }

    inline int string_convert(const wchar_t* from, char* to)
    {
#ifdef _WIN32
        int num = WideCharToMultiByte(CP_UTF8, 0, from, -1, NULL, 0, NULL, NULL);
        if (num > 0)
        {
            WideCharToMultiByte(CP_UTF8, 0, from, -1, &to[0], num, NULL, NULL);
        }
#else
        std::wstring wfrom = from;
        std::string sto = std::string(wfrom.begin(), wfrom.end());
        std::strcpy(to, sto.c_str());
        int num = sto.length();
#endif // _WIN32
        return num;
    }

    
    inline std::string utf8_to_multibyte(const std::string& utf8_str) {
#ifdef _WIN32
        int len = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, nullptr, 0);
        std::wstring wide_str(len, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, &wide_str[0], len);

        len = WideCharToMultiByte(CP_ACP, 0, wide_str.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string mb_str(len, 0);
        WideCharToMultiByte(CP_ACP, 0, wide_str.c_str(), -1, &mb_str[0], len, nullptr, nullptr);
        if( mb_str[len-1] == 0)
        {
            mb_str = mb_str.substr(0, len - 1);
        }
        return mb_str;
#else
        return utf8_str;
#endif
    }

    inline std::string encode_base64(const std::vector<uint8_t>& bytes) 
    {
        static const std::string base64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        
        std::string encoded;
        int i = 0;
        int j = 0;
        uint8_t char_array_3[3];
        uint8_t char_array_4[4];
        
        for (uint8_t byte : bytes) {
            char_array_3[i++] = byte;
            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;
                
                for (i = 0; i < 4; i++) {
                    encoded += base64_chars[char_array_4[i]];
                }
                i = 0;
            }
        }
        
        // Handle remaining bytes
        if (i) {
            for (j = i; j < 3; j++) {
                char_array_3[j] = 0;
            }
            
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            
            for (j = 0; j < i + 1; j++) {
                encoded += base64_chars[char_array_4[j]];
            }
            
            while (i++ < 3) {
                encoded += '=';
            }
        }
        
        return encoded;
    }
}
