#include "core/core.h"
#include "core/ds_log.h"
#include "text_parser.h"
#include <chrono>
#include <filesystem>

namespace diverse
{
    auto process_file(const std::string& file_path,
        IncludeProvider* include_provider,
        std::string include_context) -> std::vector<SourceChunk<std::string>>
    {
        std::set<std::string> prior_includes;
        std::set<std::string> skip_includes;
        Scanner scanner(
            "",
            "",
            prior_includes,
            skip_includes,
            include_provider,
            include_context);
        scanner.include_child(file_path, 1);
        return scanner.chunks;
    }

    auto skip_block_comment(LocationTracking it) -> std::pair<std::string, LocationTracking>
    {
        std::string s;
        while (auto value = it.next()) {
            auto c = value.value().second;
            if (c == '*') {
                s.push_back(' ');
                value = it.peek();
                if (value && value.value().second == '/') {
                    it.next();
                    s.push_back(' ');
                    break;
                }
            }
            else if (c == '\n') {
                s.push_back('\n');
            }
            else {
                s.push_back(' ');
            }
        }
        return { s, it };
    }


    Scanner::Scanner(const std::string& input,
        const std::string& thisfile,
        const std::set<std::string>& priorincludes,
        const std::set<std::string>& skipincludes,
        IncludeProvider* provider,
        const std::string& context)
        :include_provider(provider),
        include_context(context),
        this_file(thisfile),
        prior_includes(priorincludes)
    {
        input_iter = LocationTracking{ input.c_str(), input.size(), 1, 0 };
        current_chunk_first_line = 1;
    }

    auto Scanner::skip_whitespace_until_eol()->void
    {
        while (auto value = peek_char())
        {
            auto c = value->second;
            if (c == '\n')
                break;
            else if (iswspace(c))
                read_char();
            else if (c == '\\') {
                auto peek_next = input_iter;
                peek_next.next();
                auto value = peek_next.peek();
                if (value && value->second == '\n')
                {
                    read_char();
                    read_char();
                }
                else
                    break;
            }
            else if (c == '/') {
                auto next_peek = input_iter;
                next_peek.next();
                value = next_peek.peek();
                if (value && value->second == '*') {
                    read_char();
                    read_char();

                    input_iter = skip_block_comment(input_iter).second;
                }
            }
            else
                break;
        }
    }

    auto Scanner::read_string(char right_delim) ->std::string
    {
        std::string s;

        while (auto value = peek_char()) {
            auto c = value->second;
            if (c == '\n') {
                break;
            }
            else if (c == '\\') {
                read_char();
                read_char();
            }
            else if (c == right_delim) {
                read_char();
                return s;
            }
            else {
                s.push_back(c);
                read_char();
            }
        }
        return s;
    }
    auto Scanner::skip_line()->void
    {
        while (auto value = read_char()) {
            auto c = value->second;
            if ('\n' == c) {
                current_chunk.push_back('\n');
                break;
            }
            else if (c == '\\') {
                value = read_char();
                if (value && value->second == '\n') {
                    current_chunk.push_back('\n');
                }
            }
        }
    }

    auto Scanner::peek_preprocessor_ident() -> std::optional<std::pair<std::string, LocationTracking>>
    {
        std::string token;
        auto it = input_iter;
        while (auto value = it.peek()) {
            auto c = value->second;
            if ('\n' == c || '\r' == c) {
                break;
            }
            else if (isalpha(c)) {
                it.next();
                token.push_back(c);
            }
            else if (iswspace(c)) {
                if (!token.empty()) {
                    // Already found some chars, and this ends the identifier
                    break;
                }
                else {
                    // Still haven't found anything. Continue scanning.
                    it.next();
                }
            }
            else if ('\\' == c) {
                it.next();
                auto next = it.next();
                if (next && next->second == '\n') {
                    continue;
                }
                else if ('\r' == next->second && it.peek() && it.peek()->second == '\n') {
                    it.next();
                    continue;
                }
                else {
                    return {};
                }
            }
            else if ('/' == c) {
                if (!token.empty()) {
                    break;
                }
                auto next_peek = it;
                next_peek.next();
                if (next_peek.peek() && next_peek.peek()->second == '*') {
                    it.next();
                    it.next();
                    it = skip_block_comment(it).second;
                }
                else {
                    break;
                }
            }
            else {
                break;
            }
        }
        return std::optional<std::pair<std::string, LocationTracking>>({ token, it });
    }
    auto Scanner::flush_current_chunk()->void
    {
        if (!current_chunk.empty()) {
	        auto last_write_time = std::chrono::duration_cast<std::chrono::seconds>(
                std::filesystem::last_write_time(include_provider->get_working_path() + this_file).time_since_epoch()).count();
            chunks.push_back(SourceChunk<std::string>{
                current_chunk,
                    include_context,
                    this_file,
                    last_write_time,
                    static_cast<uint32>(current_chunk_first_line - 1),
            });
            current_chunk.clear();
        }
        if (auto value = peek_char()) {
            current_chunk_first_line = value->first;
        }
    }
    auto Scanner::include_child(const std::string& path, uint32 include_on_line)->void
    {
        if (prior_includes.find(path) != prior_includes.end()) {
            DS_LOG_ERROR("RecursiveInclude {} {}, {}", path, this_file, include_on_line);
            return;
        }

        flush_current_chunk();

        auto child = include_provider->resolve_path(path, include_context);
        if (skip_includes.find(child.resolved_path) != skip_includes.end())
            return;
        auto child_code = include_provider->get_include(child.resolved_path);
        prior_includes.insert(path);

        Scanner child_scan(
            child_code,
            child.resolved_path,
            prior_includes,
            skip_includes,
            include_provider,
            child.context
        );
        child_scan.process_input();

        for (auto chunk : child_scan.chunks)
            chunks.push_back(chunk);

        prior_includes.erase(path);
    }

    auto Scanner::process_input()->void
    {
        while (auto value = read_char())
        {
            auto [c_line, c] = value.value();
            switch (c)
            {
            case '/':
            {
                auto next = peek_char();
                if (next && '*' == next->second) {
                    read_char();
                    current_chunk.push_back(' ');
                    auto [white, it] = skip_block_comment(input_iter);
                    input_iter = it;
                    current_chunk.insert(current_chunk.size(), white.data());
                }
                else if (next && next->second == '/') {
                    read_char();
                    skip_line();
                }
                else {
                    current_chunk.push_back(c);
                }
            }
            break;
            case '#':
            {
                auto preprocessor_ident = peek_preprocessor_ident();
                if (preprocessor_ident && "include" == preprocessor_ident->first) {
                    input_iter = preprocessor_ident->second;
                    skip_whitespace_until_eol();

                    auto left_delim = read_char();
                    char right_delim = 0;
                    if (left_delim && left_delim->second == '"') {
                        right_delim = '"';
                    }
                    else if (left_delim && '<' == left_delim->second) {
                        right_delim = '>';
                    }

                    auto path = read_string(right_delim);
                    if (!path.empty())
                        include_child(path, c_line);
                    else {
                         DS_LOG_ERROR("ParseError: file{}, line{}", this_file, c_line);
                    }
                }
                else {
                    current_chunk.push_back(c);
                }
            }
            break;
            default:
                current_chunk.push_back(c);
                break;
            }
        }

        flush_current_chunk();
    }
}