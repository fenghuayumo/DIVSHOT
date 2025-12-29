/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include <tl/expected.hpp>
#include <type_traits>

namespace tinysog {
    namespace internal {

        // Alias tl::expected as our expected type
        template<typename T, typename E>
        using expected = tl::expected<T, E>;

        // Helper to create unexpected values  
        template<typename E>
        inline tl::unexpected<typename std::decay<E>::type> unexpected(E&& e) {
            return tl::unexpected<typename std::decay<E>::type>(std::forward<E>(e));
        }

    } // namespace internal
} // namespace tinysog


