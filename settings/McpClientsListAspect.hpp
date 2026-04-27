// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utils/aspects.h>
#include <utils/layoutbuilder.h>

namespace QodeAssist::Settings {

class McpClientsListAspect : public Utils::BaseAspect
{
    Q_OBJECT

public:
    explicit McpClientsListAspect(Utils::AspectContainer *container = nullptr);

    void addToLayoutImpl(Layouting::Layout &parent) override;
};

} // namespace QodeAssist::Settings
