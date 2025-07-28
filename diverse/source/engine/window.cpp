
#include "window.h"

namespace diverse
{
    Window* (*Window::CreateFunc)(const WindowDesc&) = NULL;

    Window::~Window()
    {
        //m_SwapChain.reset();
        //m_GraphicsContext.reset();
    }

    Window* Window::create(const WindowDesc& windowDesc)
    {
        DS_ASSERT(CreateFunc, "No Windows Create Function");
        return CreateFunc(windowDesc);
    }

    bool Window::initialise(const WindowDesc& windowDesc)
    {
        return has_initialised();
    }
}
