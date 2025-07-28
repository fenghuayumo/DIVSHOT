--* Creates an NVCC toolset for Linux.

rule 'cu'
    fileExtension { ".cu" }
    buildoutputs  { "$(OBJDIR)/%{file.objname}.cu.o" }
    buildmessage  '$(notdir $<)'
    buildcommands {'$(CXX) %{premake.modules.gmake2.cpp.fileFlags(cfg, file)} $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"'}

premake.tools.nvcc         = {}

local nvcc                 = premake.tools.nvcc
local gcc                  = premake.tools.gcc

nvcc.getcflags             = gcc.getcflags
nvcc.getcppflags           = gcc.getcppflags
nvcc.getforceincludes      = gcc.getforceincludes
nvcc.getdefines            = gcc.getdefines
nvcc.getundefines          = gcc.getundefines
nvcc.getrunpathdirs        = gcc.getrunpathdirs
nvcc.getincludedirs        = gcc.getincludedirs
nvcc.getLibraryDirectories = gcc.getLibraryDirectories
nvcc.getlinks              = gcc.getlinks
nvcc.getmakesettings       = gcc.getmakesettings

nvcc.cxxflags = {
  cudaRelocatableCode = {
    On = "-rdc=true",
    Off =  "-rdc=false"
  },
  cudaExtensibleWholeProgram = {
    On = "-ewp"
  },
  cudaFastMath = {
    On = "-use_fast_math"
  },
  cudaVerbosePTXAS = {
    On = "--ptxas-options=--verbose"
  },
  cudaKeep = {
    On = "-keep"
  },
  cudaGenLineInfo = {
    On = "-lineinfo"
  },
  kind = {
    SharedLib = "-fpic"
  }
}

function nvcc.gettoolname (cfg, tool)

  local prefix = ""
  if cfg.project.cudaPath == nil then
    prefix = "/usr/local/cuda/bin/"
  else
    prefix = cfg.project.cudaPath .. "/bin/"
  end

  if     tool == "cc" then
    name = prefix .. "nvcc"
  elseif tool == "cxx" then
    name = prefix .. "nvcc"
  elseif tool == "ar" then
    name = "ar"
  else
    name = nil
  end
  return name
end

function nvcc.getcxxflags(cfg)
  local flags = premake.config.mapFlags(cfg, nvcc.cxxflags)
  local gccFlags = premake.config.mapFlags(cfg, gcc.cxxflags)
  flags = table.join(flags,gccFlags)
  flags = table.join({"-forward-unknown-to-host-compiler"}, flags)

  if cfg.cudaCompilerOptions ~= nil then
    local cudaFlags = cfg.cudaCompilerOptions
    flags = table.join(flags, cudaFlags)
  end

  if cfg.cudaMaxRegCount ~= nil then
    flags = table.join(flags, { "--maxrregcount " .. cfg.cudaMaxRegCount})
  end

  return flags
end

function nvcc.getldflags(cfg)
  local flags = premake.config.mapFlags(cfg, gcc.ldflags)
  flags = table.join({"-forward-unknown-to-host-compiler"}, flags)
  flags = table.join({"-Xlinker=-rpath,'$$ORIGIN'"}, flags)

  if cfg.cudaLinkerOptions ~= nil then
    local cudaFlags = cfg.cudaLinkerOptions
    flags = table.join(flags, cudaFlags)
  end

  return flags
end
