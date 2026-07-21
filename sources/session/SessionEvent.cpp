// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "session/SessionEvent.hpp"

namespace QodeAssist::Session {

QString turnIdOf(const SessionEvent &event)
{
    return std::visit(
        [](const auto &alternative) -> QString {
            if constexpr (requires { alternative.turnId; })
                return alternative.turnId;
            else
                return {};
        },
        event);
}

} // namespace QodeAssist::Session
