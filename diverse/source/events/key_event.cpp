#include "core/core.h"
#include "key_event.h"
#include <sstream>

namespace diverse
{
    std::string KeyPressedEvent::ToString() const
    {
        std::stringstream ss;
        ss << "KeyPressedEvent: " << uint32_t(m_KeyCode) << " (" << m_RepeatCount << " repeats)";
        return ss.str();
    }

    std::string KeyReleasedEvent::ToString() const
    {
        std::stringstream ss;
        ss << "KeyReleasedEvent: " << uint32_t(m_KeyCode);
        return ss.str();
    }

    std::string KeyTypedEvent::ToString() const
    {
        std::stringstream ss;
        ss << "KeyTypedEvent: " << uint32_t(m_KeyCode);
        return ss.str();
    }

}