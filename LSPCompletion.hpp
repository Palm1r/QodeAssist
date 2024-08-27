/*
 * Copyright (C) 2023 The Qt Company Ltd.
 * Copyright (C) 2024 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * The Qt Company portions:
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
 *
 * Petr Mironychev portions:
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <languageserverprotocol/jsonkeys.h>
#include <languageserverprotocol/jsonrpcmessages.h>
#include <languageserverprotocol/lsptypes.h>

namespace QodeAssist {

class Completion : public LanguageServerProtocol::JsonObject
{
    static constexpr LanguageServerProtocol::Key displayTextKey{"displayText"};
    static constexpr LanguageServerProtocol::Key uuidKey{"uuid"};

public:
    using JsonObject::JsonObject;

    QString displayText() const { return typedValue<QString>(displayTextKey); }
    LanguageServerProtocol::Position position() const
    {
        return typedValue<LanguageServerProtocol::Position>(LanguageServerProtocol::positionKey);
    }
    LanguageServerProtocol::Range range() const
    {
        return typedValue<LanguageServerProtocol::Range>(LanguageServerProtocol::rangeKey);
    }
    QString text() const { return typedValue<QString>(LanguageServerProtocol::textKey); }
    void setText(const QString &text) { insert(LanguageServerProtocol::textKey, text); }
    QString uuid() const { return typedValue<QString>(uuidKey); }

    bool isValid() const override
    {
        return contains(LanguageServerProtocol::textKey)
               && contains(LanguageServerProtocol::rangeKey)
               && contains(LanguageServerProtocol::positionKey);
    }
};

class GetCompletionParams : public LanguageServerProtocol::JsonObject
{
public:
    static constexpr LanguageServerProtocol::Key docKey{"doc"};

    GetCompletionParams(const LanguageServerProtocol::TextDocumentIdentifier &document,
                        int version,
                        const LanguageServerProtocol::Position &position)
    {
        setTextDocument(document);
        setVersion(version);
        setPosition(position);
    }
    using JsonObject::JsonObject;

    // The text document.
    LanguageServerProtocol::TextDocumentIdentifier textDocument() const
    {
        return typedValue<LanguageServerProtocol::TextDocumentIdentifier>(docKey);
    }
    void setTextDocument(const LanguageServerProtocol::TextDocumentIdentifier &id)
    {
        insert(docKey, id);
    }

    // The position inside the text document.
    LanguageServerProtocol::Position position() const
    {
        return LanguageServerProtocol::fromJsonValue<LanguageServerProtocol::Position>(
            value(docKey).toObject().value(LanguageServerProtocol::positionKey));
    }
    void setPosition(const LanguageServerProtocol::Position &position)
    {
        QJsonObject result = value(docKey).toObject();
        result[LanguageServerProtocol::positionKey] = (QJsonObject) position;
        insert(docKey, result);
    }

    // The version
    int version() const { return typedValue<int>(LanguageServerProtocol::versionKey); }
    void setVersion(int version)
    {
        QJsonObject result = value(docKey).toObject();
        result[LanguageServerProtocol::versionKey] = version;
        insert(docKey, result);
    }

    bool isValid() const override
    {
        return contains(docKey)
               && value(docKey).toObject().contains(LanguageServerProtocol::positionKey)
               && value(docKey).toObject().contains(LanguageServerProtocol::versionKey);
    }
};

class GetCompletionResponse : public LanguageServerProtocol::JsonObject
{
    static constexpr LanguageServerProtocol::Key completionKey{"completions"};

public:
    using JsonObject::JsonObject;

    LanguageServerProtocol::LanguageClientArray<Completion> completions() const
    {
        return clientArray<Completion>(completionKey);
    }
};

class GetCompletionRequest
    : public LanguageServerProtocol::Request<GetCompletionResponse, std::nullptr_t, GetCompletionParams>
{
public:
    explicit GetCompletionRequest(const GetCompletionParams &params = {})
        : Request(methodName, params)
    {}
    using Request::Request;
    constexpr static const LanguageServerProtocol::Key methodName{"getCompletionsCycling"};
};
} // namespace QodeAssist
