// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "session/SessionEvent.hpp"

namespace QodeAssist::Session {

QString turnIdOf(const SessionEvent &event)
{
    return std::visit([](const auto &alternative) { return alternative.turnId; }, event);
}

} // namespace QodeAssist::Session
