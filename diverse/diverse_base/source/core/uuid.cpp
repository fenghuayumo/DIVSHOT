#include <functional>
#include "uuid.h"
#include "maths/random.h"

namespace diverse
{
    UUID::UUID()
    {
        m_UUID = Random64::Rand(1, std::numeric_limits<uint64_t>::max());
    }

    UUID::UUID(uint64_t uuid)
        : m_UUID(uuid)
    {
    }

    UUID::UUID(const UUID& other)
        : m_UUID(other.m_UUID)
    {
    }
}
