#pragma once

#include "event.h"
#include "engine/key_codes.h"

namespace diverse
{

    class DS_EXPORT KeyEvent : public Event
    {
    public:
        inline diverse::InputCode::Key GetKeyCode() const { return m_KeyCode; }

        EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)

    protected:
        KeyEvent(diverse::InputCode::Key keycode)
            : m_KeyCode(keycode)
        {
        }

        diverse::InputCode::Key m_KeyCode;
    };

    class DS_EXPORT KeyPressedEvent : public KeyEvent
    {
    public:
        KeyPressedEvent(diverse::InputCode::Key keycode, int repeatCount)
            : KeyEvent(keycode)
            , m_RepeatCount(repeatCount)
        {
        }

        inline int GetRepeatCount() const { return m_RepeatCount; }

        std::string ToString() const override;

        EVENT_CLASS_TYPE(KeyPressed)
    private:
        int m_RepeatCount;
    };

    class DS_EXPORT KeyReleasedEvent : public KeyEvent
    {
    public:
        KeyReleasedEvent(diverse::InputCode::Key keycode)
            : KeyEvent(keycode)
        {
        }

        std::string ToString() const override;

        EVENT_CLASS_TYPE(KeyReleased)
    };

    class DS_EXPORT KeyTypedEvent : public KeyEvent
    {
    public:
        KeyTypedEvent(diverse::InputCode::Key keycode, char character)
            : KeyEvent(keycode)
            , Character(character)
        {
        }

        std::string ToString() const override;

        EVENT_CLASS_TYPE(KeyTyped)

        char Character;

    private:
    };
}
