#pragma once

#ifdef DS_PLATFORM_MOBILE
#ifdef DS_PLATFORM_IOS
#include <OpenGLES/ES3/gl.h>
#include <OpenGLES/ES3/glext.h>
#elif DS_PLATFORM_ANDROID
#include <GLES3/gl3.h>
#endif

#else
#ifdef DS_PLATFORM_WINDOWS
#include <glad/glad.h>
#elif DS_PLATFORM_LINUX
#include <glad/glad.h>
#elif DS_PLATFORM_MACOS
#include <glad/glad.h>
#include <OpenGL/gl.h>
#endif

#endif
