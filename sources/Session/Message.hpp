// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <LLMQore/ContentBlocks.hpp>

#include <QString>

#include <memory>
#include <vector>

namespace QodeAssist {

class Message
{
public:
    enum class Role { System, User, Assistant };

    Message() = default;
    explicit Message(Role role, QString id = QString())
        : m_role(role)
        , m_id(std::move(id))
    {}

    Message(const Message &) = delete;
    Message &operator=(const Message &) = delete;
    Message(Message &&) noexcept = default;
    Message &operator=(Message &&) noexcept = default;
    ~Message() = default;

    Role role() const noexcept { return m_role; }
    const QString &id() const noexcept { return m_id; }
    void setId(QString id) { m_id = std::move(id); }

    const std::vector<std::unique_ptr<LLMQore::ContentBlock>> &blocks() const noexcept
    {
        return m_blocks;
    }

    void appendBlock(std::unique_ptr<LLMQore::ContentBlock> block)
    {
        if (block)
            m_blocks.push_back(std::move(block));
    }

    LLMQore::ContentBlock *lastBlock() noexcept
    {
        return m_blocks.empty() ? nullptr : m_blocks.back().get();
    }

    std::vector<std::unique_ptr<LLMQore::ContentBlock>> takeBlocks() noexcept
    {
        return std::move(m_blocks);
    }

    template<typename T>
    T *lastBlockOfType()
    {
        for (auto it = m_blocks.rbegin(); it != m_blocks.rend(); ++it) {
            if (auto *p = dynamic_cast<T *>(it->get()))
                return p;
        }
        return nullptr;
    }

    template<typename T>
    const T *lastBlockOfType() const
    {
        return const_cast<Message *>(this)->lastBlockOfType<T>();
    }

    QString text() const;

    bool hasToolUse() const;

private:
    Role m_role = Role::User;
    QString m_id;
    std::vector<std::unique_ptr<LLMQore::ContentBlock>> m_blocks;
};

} // namespace QodeAssist
