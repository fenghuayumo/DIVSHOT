--* Variables exported to the user, descriptions in the README

premake.api.register {
    name = "cudaRelocatableCode",
    scope = "config",
    kind = "boolean"
}

premake.api.register {
    name = "cudaExtensibleWholeProgram",
    scope = "config",
    kind = "boolean"
}

premake.api.register {
    name = "cudaCompilerOptions",
    scope = "config",
    kind = "table"
}

premake.api.register {
    name = "cudaLinkerOptions",
    scope = "config",
    kind = "table"
}

premake.api.register {
    name = "cudaFastMath",
    scope = "config",
    kind = "boolean"
}

premake.api.register {
    name = "cudaVerbosePTXAS",
    scope = "config",
    kind = "boolean"
}

premake.api.register {
    name = "cudaMaxRegCount",
    scope = "config",
    kind = "string"
}

premake.api.register {
    name = "cudaFiles",
    scope = "config",
    kind = "table"
}

premake.api.register {
    name = "cudaPTXFiles",
    scope = "config",
    kind = "table"
}

premake.api.register {
    name = "cudaKeep",
    scope = "config",
    kind = "boolean"
}

premake.api.register {
    name = "cudaPath",
    scope = "project",
    kind = "string"
}

premake.api.register {
    name = "cudaGenLineInfo",
    scope = "config",
    kind = "boolean"
}

premake.api.register {
    name = "cudaIntDir",
    scope = "project",
    kind = "string"
}
