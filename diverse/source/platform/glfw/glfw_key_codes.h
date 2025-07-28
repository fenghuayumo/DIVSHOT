#pragma once

#include "engine/key_codes.h"

#include <GLFW/glfw3.h>

namespace diverse
{
    namespace GLFWKeyCodes
    {
        static diverse::InputCode::Key glfw2diverseKeyboardKey(int glfwKey)
        {
            static std::map<uint32_t, diverse::InputCode::Key> keyMap = {
                { GLFW_KEY_A, diverse::InputCode::Key::A },
                { GLFW_KEY_B, diverse::InputCode::Key::B },
                { GLFW_KEY_C, diverse::InputCode::Key::C },
                { GLFW_KEY_D, diverse::InputCode::Key::D },
                { GLFW_KEY_E, diverse::InputCode::Key::E },
                { GLFW_KEY_F, diverse::InputCode::Key::F },
                { GLFW_KEY_G, diverse::InputCode::Key::G },
                { GLFW_KEY_H, diverse::InputCode::Key::H },
                { GLFW_KEY_I, diverse::InputCode::Key::I },
                { GLFW_KEY_J, diverse::InputCode::Key::J },
                { GLFW_KEY_K, diverse::InputCode::Key::K },
                { GLFW_KEY_L, diverse::InputCode::Key::L },
                { GLFW_KEY_M, diverse::InputCode::Key::M },
                { GLFW_KEY_N, diverse::InputCode::Key::N },
                { GLFW_KEY_O, diverse::InputCode::Key::O },
                { GLFW_KEY_P, diverse::InputCode::Key::P },
                { GLFW_KEY_Q, diverse::InputCode::Key::Q },
                { GLFW_KEY_R, diverse::InputCode::Key::R },
                { GLFW_KEY_S, diverse::InputCode::Key::S },
                { GLFW_KEY_T, diverse::InputCode::Key::T },
                { GLFW_KEY_U, diverse::InputCode::Key::U },
                { GLFW_KEY_V, diverse::InputCode::Key::V },
                { GLFW_KEY_W, diverse::InputCode::Key::W },
                { GLFW_KEY_X, diverse::InputCode::Key::X },
                { GLFW_KEY_Y, diverse::InputCode::Key::Y },
                { GLFW_KEY_Z, diverse::InputCode::Key::Z },

                { GLFW_KEY_0, diverse::InputCode::Key::D0 },
                { GLFW_KEY_1, diverse::InputCode::Key::D1 },
                { GLFW_KEY_2, diverse::InputCode::Key::D2 },
                { GLFW_KEY_3, diverse::InputCode::Key::D3 },
                { GLFW_KEY_4, diverse::InputCode::Key::D4 },
                { GLFW_KEY_5, diverse::InputCode::Key::D5 },
                { GLFW_KEY_6, diverse::InputCode::Key::D6 },
                { GLFW_KEY_7, diverse::InputCode::Key::D7 },
                { GLFW_KEY_8, diverse::InputCode::Key::D8 },
                { GLFW_KEY_9, diverse::InputCode::Key::D9 },

                { GLFW_KEY_F1, diverse::InputCode::Key::F1 },
                { GLFW_KEY_F2, diverse::InputCode::Key::F2 },
                { GLFW_KEY_F3, diverse::InputCode::Key::F3 },
                { GLFW_KEY_F4, diverse::InputCode::Key::F4 },
                { GLFW_KEY_F5, diverse::InputCode::Key::F5 },
                { GLFW_KEY_F6, diverse::InputCode::Key::F6 },
                { GLFW_KEY_F7, diverse::InputCode::Key::F7 },
                { GLFW_KEY_F8, diverse::InputCode::Key::F8 },
                { GLFW_KEY_F9, diverse::InputCode::Key::F9 },
                { GLFW_KEY_F10, diverse::InputCode::Key::F10 },
                { GLFW_KEY_F11, diverse::InputCode::Key::F11 },
                { GLFW_KEY_F12, diverse::InputCode::Key::F12 },

                { GLFW_KEY_MINUS, diverse::InputCode::Key::Minus },
                { GLFW_KEY_DELETE, diverse::InputCode::Key::Delete },
                { GLFW_KEY_SPACE, diverse::InputCode::Key::Space },
                { GLFW_KEY_LEFT, diverse::InputCode::Key::Left },
                { GLFW_KEY_RIGHT, diverse::InputCode::Key::Right },
                { GLFW_KEY_UP, diverse::InputCode::Key::Up },
                { GLFW_KEY_DOWN, diverse::InputCode::Key::Down },
                { GLFW_KEY_LEFT_SHIFT, diverse::InputCode::Key::LeftShift },
                { GLFW_KEY_RIGHT_SHIFT, diverse::InputCode::Key::RightShift },
                { GLFW_KEY_ESCAPE, diverse::InputCode::Key::Escape },
                { GLFW_KEY_KP_ADD, diverse::InputCode::Key::A },
                { GLFW_KEY_COMMA, diverse::InputCode::Key::Comma },
                { GLFW_KEY_BACKSPACE, diverse::InputCode::Key::Backspace },
                { GLFW_KEY_ENTER, diverse::InputCode::Key::Enter },
                { GLFW_KEY_LEFT_SUPER, diverse::InputCode::Key::LeftSuper },
                { GLFW_KEY_RIGHT_SUPER, diverse::InputCode::Key::RightSuper },
                { GLFW_KEY_LEFT_ALT, diverse::InputCode::Key::LeftAlt },
                { GLFW_KEY_RIGHT_ALT, diverse::InputCode::Key::RightAlt },
                { GLFW_KEY_LEFT_CONTROL, diverse::InputCode::Key::LeftControl },
                { GLFW_KEY_RIGHT_CONTROL, diverse::InputCode::Key::RightControl },
                { GLFW_KEY_TAB, diverse::InputCode::Key::Tab }
            };

            return keyMap[glfwKey];
        }

        static diverse::InputCode::MouseKey glfw2diverseMouseKey(int glfwKey)
        {

            static std::map<uint32_t, diverse::InputCode::MouseKey> keyMap = {
                { GLFW_MOUSE_BUTTON_LEFT, diverse::InputCode::MouseKey::ButtonLeft },
                { GLFW_MOUSE_BUTTON_RIGHT, diverse::InputCode::MouseKey::ButtonRight },
                { GLFW_MOUSE_BUTTON_MIDDLE, diverse::InputCode::MouseKey::ButtonMiddle }
            };

            return keyMap[glfwKey];
        }
    }
}
