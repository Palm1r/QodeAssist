// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <functional>
#include <utility>
#include <vector>

namespace LLMQore {
class ToolsManager;
}

namespace QodeAssist {

class ToolContributorRegistry
{
public:
    using Contributor = std::function<void(LLMQore::ToolsManager *)>;

    void add(Contributor contributor)
    {
        if (contributor)
            m_contributors.push_back(std::move(contributor));
    }

    void contribute(LLMQore::ToolsManager *tools) const
    {
        if (!tools)
            return;
        for (const auto &contributor : m_contributors)
            contributor(tools);
    }

    void clear() { m_contributors.clear(); }

private:
    std::vector<Contributor> m_contributors;
};

} // namespace QodeAssist
