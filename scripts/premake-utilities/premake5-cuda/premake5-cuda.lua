require("src/cuda-exported-variables")

if os.target() == "windows" then
    dofile("src/premake5-cuda-vs.lua")
elseif os.target() == "linux" then
    dofile("src/premake5-cuda-nvcc.lua")
end
