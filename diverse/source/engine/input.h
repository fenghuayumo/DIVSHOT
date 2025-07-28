#pragma once
#include "events/event.h"
#include "events/key_event.h"
#include "events/mouse_event.h"
#include "utility/singleton.h"
#include "engine/key_codes.h"
#include <glm/vec2.hpp>

#define MAX_KEYS 1024
#define MAX_BUTTONS 32

namespace diverse
{
    class Event;

    enum class MouseMode
    {
        Visible,
        Hidden,
        Captured
    };

    struct Controller
    {
        int ID;
        std::string Name;
        std::map<int, bool> ButtonStates;
        std::map<int, float> AxisStates;
        std::map<int, uint8_t> HatStates;
    };

    class DS_EXPORT Input : public ThreadSafeSingleton<Input>
    {
        friend class TSingleton<Input>;

    public:
        Input();
        virtual ~Input() = default;

        bool get_key_pressed(diverse::InputCode::Key key) const { return m_KeyPressed[int(key)]; }
        bool get_key_held(diverse::InputCode::Key key) const { return m_KeyHeld[int(key)]; }
        bool get_mouse_clicked(diverse::InputCode::MouseKey key) const { return m_MouseClicked[int(key)]; }
        bool get_mouse_held(diverse::InputCode::MouseKey key) const { return m_MouseHeld[int(key)]; }

        void set_key_pressed(diverse::InputCode::Key key, bool a) { m_KeyPressed[int(key)] = a; }
        void set_key_held(diverse::InputCode::Key key, bool a) { m_KeyHeld[int(key)] = a; }
        void set_mouse_clicked(diverse::InputCode::MouseKey key, bool a) { m_MouseClicked[int(key)] = a; }
        void set_mouse_held(diverse::InputCode::MouseKey key, bool a) { m_MouseHeld[int(key)] = a; }

        void set_mouse_on_screen(bool onScreen) { m_MouseOnScreen = onScreen; }
        bool get_mouse_on_screen() const { return m_MouseOnScreen; }

        void store_mouse_position(float xpos, float ypos) { m_MousePosition = glm::vec2(float(xpos), float(ypos)); }
        const glm::vec2& get_mouse_position() const { return m_MousePosition; }
        const glm::vec2& get_mouse_delta() const {return m_MouseDelta;}

        void set_scroll_offset(float offset) { m_ScrollOffset = offset; }
        float get_scroll_offset() const { return m_ScrollOffset; }

        void reset();
        void reset_pressed();
        void on_event(Event& e);

        MouseMode get_mouse_mode() const { return m_MouseMode; }
        void set_mouse_mode(MouseMode mode) { m_MouseMode = mode; }

        // Controllers
        bool is_controller_present(int id);
        std::vector<int> get_connected_controller_ids();
        Controller* get_controller(int id);
        Controller* get_or_add_controller(int id);

        std::string get_controller_name(int id);
        bool is_controller_button_pressed(int controllerID, int button);
        float get_controller_axis(int controllerID, int axis);
        uint8_t get_controller_hat(int controllerID, int hat);
        void remove_controller(int id);

        std::map<int, Controller>& get_controllers() { return m_Controllers; }

    private:
    protected:
        bool on_key_pressed(KeyPressedEvent& e);
        bool on_key_released(KeyReleasedEvent& e);
        bool on_mouse_pressed(MouseButtonPressedEvent& e);
        bool on_mouse_released(MouseButtonReleasedEvent& e);
        bool on_mouse_scrolled(MouseScrolledEvent& e);
        bool on_mouse_moved(MouseMovedEvent& e);
        bool on_mouse_enter(MouseEnterEvent& e);

        bool m_KeyPressed[MAX_KEYS];
        bool m_KeyHeld[MAX_KEYS];

        bool m_MouseHeld[MAX_BUTTONS];
        bool m_MouseClicked[MAX_BUTTONS];

        float m_ScrollOffset = 0.0f;

        bool m_MouseOnScreen;
        MouseMode m_MouseMode;

        glm::vec2 m_MousePosition;
        glm::vec2 m_MouseDelta;
        glm::vec2 m_PreviousMousePosition;
        
        std::map<int, Controller> m_Controllers;
    };
}
