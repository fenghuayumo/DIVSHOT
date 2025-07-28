#pragma once
#include <diverse.h>
#include <engine/entry_point.h>
#include "editor.h"

diverse::Application* diverse::createApplication()
{
    return new Editor();
}
