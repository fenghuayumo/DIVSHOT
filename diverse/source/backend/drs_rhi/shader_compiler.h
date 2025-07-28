#pragma once
#include <string>
#include <vector>
#include <utility/text_parser.h>

namespace diverse
{
	struct CompiledShader
	{
		std::string name;
		std::vector<uint8>	spriv;
	};

	struct CompilerInput
	{
		std::string shader_source_filename;
		std::string entry_point = "main";
		const char* target_profile = nullptr;
		std::vector<const char*> defines;
	};

	struct CompilerOutput
	{
		std::shared_ptr<void> internal_state;
		inline bool is_valid() const { return internal_state.get() != nullptr; }

		const uint8_t* shaderdata = nullptr;
		size_t shadersize = 0;
		std::vector<uint8_t> shaderhash;
		std::string error_message;
		std::vector<std::string> dependencies;
	};

	//CompilerOutput	CompileHlsl(const std::filesystem::path& source_name,
	//	const std::string& entry_point,
	//	const std::string& target_profile,
	//	const std::vector<const char*>& macro_defines);

	//auto compile_generic_shader_hlsl(
	//	const CompilerInput& input
	//)->std::vector<uint8>;
	auto compile_generic_shader_hlsl(
			const std::string& name, 
			const std::string& source,
			i64 last_write_time, 
			const char* target_profile,
			const std::string& entry_point) -> std::vector<uint8>;

	auto compile_generic_shader_hlsl(
			const std::string& name, 
			const std::vector<SourceChunk<std::string>>& source, 
			const char* target_profile, 
			const std::vector<std::pair<std::string, std::string>>& defines = {},
			const std::string& entry_point = "main") -> std::vector<uint8>;

	bool shader_compiler_init();
}