#include "core/core.h"
#include <algorithm>
#include <vector>
#include <sstream>
#include "string_utils.h"
#include <cctype>

#ifdef DS_PLATFORM_WINDOWS
#include <windows.h>
#include <DbgHelp.h>
#else
#include <cxxabi.h> // __cxa_demangle()
#endif

#include <iomanip>

namespace diverse
{
    namespace stringutility
    {
        std::string get_file_extension(const std::string& FileName)
        {
            auto pos = FileName.find_last_of('.');
            if(pos != std::string::npos)
                return FileName.substr(pos + 1);
            return "";
        }

        std::string remove_file_extension(const std::string& FileName)
        {
            auto pos = FileName.find_last_of('.');
            if(pos != std::string::npos)
                return FileName.substr(0, pos);
            return FileName;
        }

        std::string get_file_name(const std::string& FilePath)
        {
            auto pos = FilePath.find_last_of('/');
            if(pos != std::string::npos)
                return FilePath.substr(pos + 1);

            pos = FilePath.find_last_of("\\");
            if(pos != std::string::npos)
                return FilePath.substr(pos + 1);

            return FilePath;
        }

        std::string to_lower(const std::string& text)
        {
            std::string lowerText = text;
            std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);
            return lowerText;
        }

        std::string get_file_location(const std::string& FilePath)
        {
            auto pos = FilePath.find_last_of('/');
            if(pos != std::string::npos)
                return FilePath.substr(0, pos + 1);

            pos = FilePath.find_last_of("\\");
            if(pos != std::string::npos)
                return FilePath.substr(0, pos + 1);

            return FilePath;
        }

        std::string remove_name(const std::string& FilePath)
        {
            auto pos = FilePath.find_last_of('/');
            if(pos != std::string::npos)
                return FilePath.substr(0, pos + 1);

            pos = FilePath.find_last_of("\\");
            if(pos != std::string::npos)
                return FilePath.substr(0, pos + 1);

            return FilePath;
        }

        bool is_hidden_file(const std::string& path)
        {
            if(path != ".." && path != "." && path[0] == '.')
            {
                return true;
            }

            return false;
        }

        std::vector<std::string> split_string(const std::string& string, const std::string& delimiters)
        {
            size_t start = 0;
            size_t end   = string.find_first_of(delimiters);

            std::vector<std::string> result;

            while(end <= std::string::npos)
            {
                std::string token = string.substr(start, end - start);
                if(!token.empty())
                    result.push_back(token);

                if(end == std::string::npos)
                    break;

                start = end + 1;
                end   = string.find_first_of(delimiters, start);
            }

            return result;
        }

        std::vector<std::string> split_string(const std::string& string, const char delimiter)
        {
            return split_string(string, std::string(1, delimiter));
        }

        std::vector<std::string> Tokenize(const std::string& string)
        {
            return split_string(string, " \t\n");
        }

        std::vector<std::string> get_lines(const std::string& string)
        {
            return split_string(string, "\n");
        }

        const char* find_token(const char* str, const std::string& token)
        {
            const char* t = str;
            while((t = strstr(t, token.c_str())))
            {
                bool left  = str == t || isspace(t[-1]);
                bool right = !t[token.size()] || isspace(t[token.size()]);
                if(left && right)
                    return t;

                t += token.size();
            }
            return nullptr;
        }

        const char* find_token(const std::string& string, const std::string& token)
        {
            return find_token(string.c_str(), token);
        }

        int32_t find_string_position(const std::string& string, const std::string& search, uint32_t offset)
        {
            const char* str   = string.c_str() + offset;
            const char* found = strstr(str, search.c_str());
            if(found == nullptr)
                return -1;
            return (int32_t)(found - str) + offset;
        }

        std::string string_range(const std::string& string, uint32_t start, uint32_t length)
        {
            return string.substr(start, length);
        }

        std::string remove_string_range(const std::string& string, uint32_t start, uint32_t length)
        {
            std::string result = string;
            return result.erase(start, length);
        }

        std::string get_block(const char* str, const char** outPosition)
        {
            const char* end = strstr(str, "}");
            if(!end)
                return std::string(str);

            if(outPosition)
                *outPosition = end;
            const uint32_t length = static_cast<uint32_t>(end - str + 1);
            return std::string(str, length);
        }

        std::string get_block(const std::string& string, uint32_t offset)
        {
            const char* str = string.c_str() + offset;
            return stringutility::get_block(str);
        }

        std::string get_statement(const char* str, const char** outPosition)
        {
            const char* end = strstr(str, ";");
            if(!end)
                return std::string(str);

            if(outPosition)
                *outPosition = end;
            const uint32_t length = static_cast<uint32_t>(end - str + 1);
            return std::string(str, length);
        }

        bool string_contains(const std::string& string, const std::string& chars)
        {
            return string.find(chars) != std::string::npos;
        }

        bool starts_with(const std::string& string, const std::string& start)
        {
            return string.find(start) == 0;
        }

        int32_t next_int(const std::string& string)
        {
            for(uint32_t i = 0; i < string.size(); i++)
            {
                if(isdigit(string[i]))
                    return atoi(&string[i]);
            }
            return -1;
        }

        bool string_equals(const std::string& string1, const std::string& string2)
        {
            return strcmp(string1.c_str(), string2.c_str()) == 0;
        }

        std::string string_replace(std::string str, char ch1, char ch2)
        {
            for(int i = 0; i < str.length(); ++i)
            {
                if(str[i] == ch1)
                    str[i] = ch2;
            }

            return str;
        }

        std::string string_replace(std::string str, char ch)
        {
            for(int i = 0; i < str.length(); ++i)
            {
                if(str[i] == ch)
                {
                    str = std::string(str).substr(0, i) + std::string(str).substr(i + 1, str.length());
                }
            }

            return str;
        }

        std::string& back_slashes_2_slashes(std::string& string)
        {
            size_t len = string.length();
            for(size_t i = 0; i < len; i++)
            {
                if(string[i] == '\\')
                {
                    string[i] = '/';
                }
            }
            return string;
        }

        std::string& slashes_2_back_slashes(std::string& string)
        {
            size_t len = string.length();
            for(size_t i = 0; i < len; i++)
            {
                if(string[i] == '/')
                {
                    string[i] = '\\';
                }
            }
            return string;
        }

        std::string& remove_spaces(std::string& string)
        {
            std::string::iterator endIterator = std::remove(string.begin(), string.end(), ' ');
            string.erase(endIterator, string.end());
            string.erase(std::remove_if(string.begin(),
                                        string.end(),
                                        [](unsigned char x)
                                        {
                                            return std::isspace(x);
                                        }),
                         string.end());

            return string;
        }

        std::string& remove_character(std::string& string, const char character)
        {
            std::string::iterator endIterator = std::remove(string.begin(), string.end(), character);
            string.erase(endIterator, string.end());
            string.erase(std::remove_if(string.begin(),
                                        string.end(),
                                        [](unsigned char x)
                                        {
                                            return std::isspace(x);
                                        }),
                         string.end());

            return string;
        }

        std::string demangle(const std::string& string)
        {
            if(string.empty())
                return {};

#if defined(DS_PLATFORM_WINDOWS)
            char undecorated_name[1024];
            if(!UnDecorateSymbolName(
                   string.c_str(), undecorated_name, sizeof(undecorated_name),
                   UNDNAME_COMPLETE))
            {
                return string;
            }
            else
            {
                return std::string(undecorated_name);
            }
#else
            char* demangled = nullptr;
            int status      = -1;
            demangled       = abi::__cxa_demangle(string.c_str(), nullptr, nullptr, &status);
            std::string ret = status == 0 ? std::string(demangled) : string;
            free(demangled);
            return ret;
#endif
        }

        std::string byte_2_string(uint64_t bytes)
        {
            static const float gb = 1024 * 1024 * 1024;
            static const float mb = 1024 * 1024;
            static const float kb = 1024;

            std::stringstream result;
            if(bytes > gb)
                result << std::fixed << std::setprecision(2) << (float)bytes / gb << " gb";
            else if(bytes > mb)
                result << std::fixed << std::setprecision(2) << (float)bytes / mb << " mb";
            else if(bytes > kb)
                result << std::fixed << std::setprecision(2) << (float)bytes / kb << " kb";
            else
                result << std::fixed << std::setprecision(2) << (float)bytes << " bytes";

            return result.str();
        }

        String8 str8_skip_whitespace(String8 str)
        {
            uint64_t first_non_ws = 0;
            for(uint64_t idx = 0; idx < str.size; idx += 1)
            {
                first_non_ws = idx;
                if(!CharIsSpace(str.str[idx]))
                {
                    break;
                }
                else if(idx == str.size - 1)
                {
                    first_non_ws = 1;
                }
            }
            return Substr8(str, RangeU64({ first_non_ws, str.size }));
        }

        String8 str8_chop_whitespace(String8 str)
        {
            uint64_t first_ws_at_end = str.size;
            for(uint64_t idx = str.size - 1; idx < str.size; idx -= 1)
            {
                if(!CharIsSpace(str.str[idx]))
                {
                    break;
                }
                first_ws_at_end = idx;
            }
            return Substr8(str, RangeU64({ 0, first_ws_at_end }));
        }

        String8 str8_skip_chop_whitespace(String8 str)
        {
            return str8_skip_whitespace(str8_chop_whitespace(str));
        }

        String8 str8_skip_chop_newlines(String8 str)
        {
            uint64_t first_non_ws = 0;
            for(uint64_t idx = 0; idx < str.size; idx += 1)
            {
                first_non_ws = idx;
                if(str.str[idx] != '\n' && str.str[idx] != '\r')
                {
                    break;
                }
            }

            uint64_t first_ws_at_end = str.size;
            for(uint64_t idx = str.size - 1; idx < str.size; idx -= 1)
            {
                if(str.str[idx] != '\n' && str.str[idx] != '\r')
                {
                    break;
                }
                first_ws_at_end = idx;
            }

            return Substr8(str, RangeU64({ first_non_ws, first_ws_at_end }));
        }

        String8 str8_path_chop_last_period(String8 string)
        {
            uint64_t periodPos = FindSubstr8(string, Str8Lit("."), 0, MatchFlags::FindLast);
            if(periodPos < string.size)
            {
                string.size = periodPos;
            }
            return string;
        }

        String8 str8_path_skip_last_slash(String8 string)
        {
            uint64_t slash_pos = FindSubstr8(string, Str8Lit("/"), 0, MatchFlags(MatchFlags::SlashInsensitive | MatchFlags::FindLast));
            if(slash_pos < string.size)
            {
                string.str += slash_pos + 1;
                string.size -= slash_pos + 1;
            }
            return string;
        }

        String8 str8Path_chop_last_slash(String8 string)
        {
            uint64_t slash_pos = FindSubstr8(string, Str8Lit("/"), 0, MatchFlags(MatchFlags::SlashInsensitive | MatchFlags::FindLast));
            if(slash_pos < string.size)
            {
                string.size = slash_pos;
            }
            return string;
        }

        String8 str8_path_skip_last_period(String8 string)
        {
            uint64_t period_pos = FindSubstr8(string, Str8Lit("."), 0, MatchFlags::FindLast);
            if(period_pos < string.size)
            {
                string.str += period_pos + 1;
                string.size -= period_pos + 1;
            }
            return string;
        }

        String8 str8_path_chop_past_last_slash(String8 string)
        {
            uint64_t slash_pos = FindSubstr8(string, Str8Lit("/"), 0, MatchFlags(MatchFlags::SlashInsensitive | MatchFlags::FindLast));
            if(slash_pos < string.size)
            {
                string.size = slash_pos + 1;
            }
            return string;
        }

        PathType path_type_from_str8(String8 path)
        {
            PathType kind = PathType::Relative;
            if(path.size >= 1 && path.str[0] == '/')
            {
                kind = PathType::RootAbsolute;
            }
            if(path.size >= 2 && CharIsAlpha(path.str[0]) && path.str[1] == ':')
            {
                kind = PathType::DriveAbsolute;
            }
            return kind;
        }

        String8List path_parts_from_str8(Arena* arena, String8 path)
        {
            String8 splits[] = { Str8Lit("/"), Str8Lit("\\") };
            String8List strs = StrSplit8(arena, path, ArrayCount(splits), splits);
            return strs;
        }

        String8List absolute_path_parts_from_source_parts_type(Arena* arena, String8 source, String8List parts, PathType type)
        {
            if(type == PathType::Relative)
            {
                String8List concattedParts = { 0 };
                String8List sourceParts    = path_parts_from_str8(arena, source);
                Str8ListConcatInPlace(&concattedParts, &sourceParts);
                Str8ListConcatInPlace(&concattedParts, &parts);
                parts = concattedParts;
            }

            return parts;
        }

        String8List dot_resolved_path_parts_from_parts(Arena* arena, String8List parts)
        {
            ArenaTemp scratch = ArenaTempBegin(arena);
            struct NodeNode
            {
                NodeNode* next;
                String8Node* node;
            };

            NodeNode* part_stack_top = 0;
            for(String8Node* n = parts.first; n != 0; n = n->next)
            {
                if(Str8Match(n->string, Str8Lit(".."), MatchFlags(0)))
                {
                    StackPop(part_stack_top);
                }
                else if(Str8Match(n->string, Str8Lit("."), MatchFlags(0)))
                {
                }
                else
                {
                    NodeNode* nn = PushArray(scratch.arena, NodeNode, 1);
                    nn->node     = n;
                    StackPush(part_stack_top, nn);
                }
            }
            String8List result = { 0 };
            for(NodeNode* nn = part_stack_top; nn != 0; nn = nn->next)
            {
                Str8ListPushFront(arena, &result, nn->node->string);
            }
            ArenaTempEnd(scratch);
            return result;
        }

        String8 normalized_path_from_str8(Arena* arena, String8 source, String8 path)
        {
            ArenaTemp scratch                        = ArenaTempBegin(arena);
            path                                     = str8_skip_whitespace(path);
            bool trailing_slash                      = path.size > 0 && (path.str[path.size - 1] == '/' || path.str[path.size - 1] == '\\');
            PathType type                            = path_type_from_str8(path);
            String8List path_parts                   = path_parts_from_str8(scratch.arena, path);
            String8List absolute_path_parts          = absolute_path_parts_from_source_parts_type(scratch.arena, source, path_parts, type);
            String8List absolute_resolved_path_parts = dot_resolved_path_parts_from_parts(scratch.arena, absolute_path_parts);
            StringJoin join                          = { 0 };
            join.sep                                 = Str8Lit("/");
            if(trailing_slash)
            {
                join.post = Str8Lit("/");
            }
            String8 absolute_resolved_path = Str8ListJoin(scratch.arena, absolute_resolved_path_parts, &join);
            ArenaTempEnd(scratch);
            return absolute_resolved_path;
        }

        String8 get_file_name(String8 str, bool directory)
        {
            if(directory)
                return str8_path_skip_last_slash(str);
            else
                return str8_path_skip_last_slash(str8_path_chop_last_period(str));
        }

        uint64_t basic_hash_from_string(String8 string)
        {
            uint64_t result = 5381;
            for(uint64_t i = 0; i < string.size; i += 1)
            {
                result = ((result << 5) + result) + string.str[i];
            }
            return result;
        }
    }
}
