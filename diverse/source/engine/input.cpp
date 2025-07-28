#include <map>
#include "core/core.h"
#include "input.h"

namespace diverse
{
    Input::Input()
        : m_MouseMode(MouseMode::Visible)
    {
        reset();
    }

    void Input::reset()
    {
        memset(m_KeyHeld, 0, MAX_KEYS);
        memset(m_KeyPressed, 0, MAX_KEYS);
        memset(m_MouseClicked, 0, MAX_BUTTONS);
        memset(m_MouseHeld, 0, MAX_BUTTONS);

        m_MouseOnScreen = true;
        m_ScrollOffset  = 0.0f;
        m_MouseDelta = glm::vec2(0.0f);
    }

    void Input::reset_pressed()
    {
        memset(m_KeyPressed, 0, MAX_KEYS);
        memset(m_MouseClicked, 0, MAX_BUTTONS);
        m_ScrollOffset = 0;
        m_MouseDelta = glm::vec2(0.0f);
    }

    void Input::on_event(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>(BIND_EVENT_FN(Input::on_key_pressed));
        dispatcher.Dispatch<KeyReleasedEvent>(BIND_EVENT_FN(Input::on_key_released));
        dispatcher.Dispatch<MouseButtonPressedEvent>(BIND_EVENT_FN(Input::on_mouse_pressed));
        dispatcher.Dispatch<MouseButtonReleasedEvent>(BIND_EVENT_FN(Input::on_mouse_released));
        dispatcher.Dispatch<MouseScrolledEvent>(BIND_EVENT_FN(Input::on_mouse_scrolled));
        dispatcher.Dispatch<MouseMovedEvent>(BIND_EVENT_FN(Input::on_mouse_moved));
        dispatcher.Dispatch<MouseEnterEvent>(BIND_EVENT_FN(Input::on_mouse_enter));
    }

    bool Input::on_key_pressed(KeyPressedEvent& e)
    {
        set_key_pressed(diverse::InputCode::Key(e.GetKeyCode()), e.GetRepeatCount() < 1);
        set_key_held(diverse::InputCode::Key(e.GetKeyCode()), true);
        return false;
    }

    bool Input::on_key_released(KeyReleasedEvent& e)
    {
        set_key_pressed(diverse::InputCode::Key(e.GetKeyCode()), false);
        set_key_held(diverse::InputCode::Key(e.GetKeyCode()), false);
        return false;
    }

    bool Input::on_mouse_pressed(MouseButtonPressedEvent& e)
    {
        set_mouse_clicked(diverse::InputCode::MouseKey(e.GetMouseButton()), true);
        set_mouse_held(diverse::InputCode::MouseKey(e.GetMouseButton()), true);
        return false;
    }

    bool Input::on_mouse_released(MouseButtonReleasedEvent& e)
    {
        set_mouse_clicked(diverse::InputCode::MouseKey(e.GetMouseButton()), false);
        set_mouse_held(diverse::InputCode::MouseKey(e.GetMouseButton()), false);
        return false;
    }

    bool Input::on_mouse_scrolled(MouseScrolledEvent& e)
    {
        set_scroll_offset(e.GetYOffset());
        return false;
    }

    bool Input::on_mouse_moved(MouseMovedEvent& e)
    {
        static bool firstTime = true;
        if( firstTime )
        {
            m_PreviousMousePosition = {e.GetX(), e.GetY()};
            store_mouse_position(e.GetX(), e.GetY());
            m_MouseDelta = m_MousePosition - m_PreviousMousePosition;
            firstTime = false;
            return false;
        }
        m_MouseDelta = m_MousePosition - m_PreviousMousePosition;
        m_PreviousMousePosition = m_MousePosition;
        store_mouse_position(e.GetX(), e.GetY());
        return false;
    }

    bool Input::on_mouse_enter(MouseEnterEvent& e)
    {
        set_mouse_on_screen(e.GetEntered());
        return false;
    }

    bool Input::is_controller_present(int id)
    {
        return m_Controllers.find(id) != m_Controllers.end();
    }

    std::vector<int> Input::get_connected_controller_ids()
    {
        std::vector<int> ids;
        ids.reserve(m_Controllers.size());
        for(auto [id, controller] : m_Controllers)
            ids.emplace_back(id);

        return ids;
    }

    Controller* Input::get_controller(int id)
    {
        if(!Input::is_controller_present(id))
            return nullptr;

        return &m_Controllers.at(id);
    }

    Controller* Input::get_or_add_controller(int id)
    {
        return &(m_Controllers[id]);
    }

    std::string Input::get_controller_name(int id)
    {
        if(!Input::is_controller_present(id))
            return {};

        return m_Controllers.at(id).Name;
    }

    bool Input::is_controller_button_pressed(int controllerID, int button)
    {
        if(!Input::is_controller_present(controllerID))
            return false;

        const Controller& controller = m_Controllers.at(controllerID);
        if(controller.ButtonStates.find(button) == controller.ButtonStates.end())
            return false;

        return controller.ButtonStates.at(button);
    }

    float Input::get_controller_axis(int controllerID, int axis)
    {
        if(!Input::is_controller_present(controllerID))
            return 0.0f;

        const Controller& controller = m_Controllers.at(controllerID);
        if(controller.AxisStates.find(axis) == controller.AxisStates.end())
            return 0.0f;

        return controller.AxisStates.at(axis);
    }

    uint8_t Input::get_controller_hat(int controllerID, int hat)
    {
        if(!Input::is_controller_present(controllerID))
            return 0;

        const Controller& controller = m_Controllers.at(controllerID);
        if(controller.HatStates.find(hat) == controller.HatStates.end())
            return 0;

        return controller.HatStates.at(hat);
    }

    void Input::remove_controller(int id)
    {
        for(auto it = m_Controllers.begin(); it != m_Controllers.end();)
        {
            int currentID = it->first;
            if(currentID == id)
            {
                m_Controllers.erase(it);
                return;
            }
            else
                it++;
        }
    }
}
