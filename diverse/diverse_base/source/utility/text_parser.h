#pragma once
#include <string>
#include <set>
#include <optional>
#include <unordered_map>

namespace diverse
{
    template<typename IncludeContext>
    struct ResolvedInclude
    {
        std::string resolved_path;
        IncludeContext context;
    };

    struct IncludeProvider
    {
        virtual auto resolve_path(const std::string& path, const std::string context) -> ResolvedInclude<std::string> = 0;
        virtual auto get_include(const std::string& path) -> std::string = 0;
        virtual auto get_working_path()-> std::string = 0;
    };

    template<typename IncludeContext>
    struct SourceChunk
    {
        /// Source text
        std::string source;

        /// Context from the include provider
        IncludeContext context;
        /// File the code came from; only the leaf of the path.
        /// For nested file information, use a custom `IncludeContext`.
        std::string file;
        long long last_write_time = 0;
        /// Line in the `file` at which this snippet starts
        u32 line_offset;
    };

    auto process_file(const std::string& file_path,
        IncludeProvider* include_provider,
        std::string include_context) -> std::vector<SourceChunk<std::string>>;

    struct LocationTracking
    {
        const char* chars;
        size_t len = 0;
        u32 line = 0;
        u32 loc = 0;

        std::optional<std::pair<u32, char>> peek()
        {
            if (loc < len)
                return std::optional<std::pair<u32, char>>({ line, chars[loc] });
            return {};
        }

        std::optional<std::pair<u32, char>>  next()
        {
            auto a = chars[loc++];
            auto n1 = a == '\n';
            if (n1) line++;
            if (loc <= len)
                return std::optional<std::pair<u32, char>>({ line, a });
            return {};
        }
    };

    struct Scanner
    {
        IncludeProvider* include_provider;
        std::string include_context;
        std::string this_file;
        LocationTracking input_iter;
        std::set<std::string>   prior_includes;
        std::set<std::string>   skip_includes;
        std::vector<SourceChunk<std::string>>   chunks;
        std::string current_chunk;
        u32 current_chunk_first_line;

        Scanner(const std::string& input,
            const std::string& this_file,
            const std::set<std::string>& prior_includes,
            const std::set<std::string>& skip_includes,
            IncludeProvider* include_provider,
            const std::string& include_context);

        auto read_char() -> std::optional<std::pair<u32, char>>
        {
            return input_iter.next();
        }

        auto peek_char() -> std::optional<std::pair<u32, char>>
        {
            return input_iter.peek();
        }

        auto skip_whitespace_until_eol() -> void;
        auto read_string(char right_delim) -> std::string;
        auto skip_line() -> void;
        auto peek_preprocessor_ident() -> std::optional<std::pair<std::string, LocationTracking>>;
        auto flush_current_chunk() -> void;
        auto include_child(const std::string& path, u32 include_on_line) -> void;
        auto process_input() -> void;
    };


}