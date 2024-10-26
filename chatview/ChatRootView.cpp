/*
 * Copyright (C) 2024 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
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

#include "ChatRootView.hpp"
#include <QtGui/qclipboard.h>
#include <texteditor/texteditor.h>
#include <utils/theme/theme.h>
#include <utils/utilsicons.h>

#include "CodeChunker.h"
#include "EmbeddingsGenerator.h"
#include "EmbeddingsStorage.hpp"
#include "GeneralSettings.hpp"

namespace QodeAssist::Chat {

ChatRootView::ChatRootView(QQuickItem *parent)
    : QQuickItem(parent)
    , m_chatModel(new ChatModel(this))
    , m_clientInterface(new ClientInterface(m_chatModel, this))
{
    auto &settings = Settings::generalSettings();

    connect(&settings.chatModelName,
            &Utils::BaseAspect::changed,
            this,
            &ChatRootView::currentTemplateChanged);

    connect(m_clientInterface, &ClientInterface::isStarted, this, [this]() {
        setIsAnswering(true);
    });
    connect(m_clientInterface, &ClientInterface::isCompleted, this, [this]() {
        setIsAnswering(false);
    });

    generateColors();
}

ChatModel *ChatRootView::chatModel() const
{
    return m_chatModel;
}

QColor ChatRootView::backgroundColor() const
{
    return Utils::creatorColor(Utils::Theme::BackgroundColorNormal);
}

void ChatRootView::sendMessage(const QString &message) const
{
    m_clientInterface->sendMessage(message);
}

void ChatRootView::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

void ChatRootView::cancelRequest()
{
    m_clientInterface->cancelRequest();
}

void ChatRootView::generateChunks()
{
    TextEditor::BaseTextEditor *editor = TextEditor::BaseTextEditor::currentTextEditor();
    if (!editor) {
        qDebug() << "No active editor";
        return;
    }

    TextEditor::TextDocument *textDocument = editor->textDocument();
    if (!textDocument) {
        qDebug() << "No text document";
        return;
    }

    // Проверяем, не устарели ли эмбеддинги
    auto &storage = LLMCore::EmbeddingsStorage::instance();
    // if (storage.isFileUpToDate(textDocument->filePath().toString())) {
    //     qDebug() << "Embeddings are up to date for" << textDocument->filePath().toString();
    //     return;
    // }

    // Разбиваем документ на чанки
    auto &chunker = LLMCore::CodeChunker::instance();
    auto chunks = chunker.splitDocument(textDocument);
    qDebug() << "Generated" << chunks.size() << "chunks";

    for (const auto &chunk : chunks) {
        qDebug() << "Chunk from file:" << chunk.filePath << "Lines:" << chunk.startLine << "-"
                 << chunk.endLine << "Content size:" << chunk.content.length()
                 << "Has overlap:" << !chunk.overlapContent.isEmpty();
    }

    // Генерируем эмбеддинги для чанков
    auto &generator = LLMCore::EmbeddingsGenerator::instance();

    // Сначала отключаем старые соединения, если они есть
    disconnect(&generator, nullptr, this, nullptr);

    // Подключаем обработчик для чанков кода
    connect(&generator,
            &LLMCore::EmbeddingsGenerator::embeddingGenerated,
            this,
            [](const LLMCore::CodeChunk &chunk, const QVector<float> &embedding) {
                qDebug() << "Generated embedding for chunk at lines" << chunk.startLine << "-"
                         << chunk.endLine;

                // Сохраняем эмбеддинг
                LLMCore::EmbeddingsStorage::instance().storeEmbedding(chunk, embedding);
            });

    // Подключаем обработчик ошибок
    connect(&generator, &LLMCore::EmbeddingsGenerator::error, this, [](const QString &error) {
        qDebug() << "Embedding error:" << error;
    });

    // Генерируем эмбеддинги для документа
    generator.generateEmbeddings(chunks);
}

void ChatRootView::sendEmbeddings(const QString &query)
{
    auto &generator = LLMCore::EmbeddingsGenerator::instance();
    auto &storage = LLMCore::EmbeddingsStorage::instance();

    // Отключаем предыдущие соединения
    disconnect(&generator, nullptr, this, nullptr);

    // Подключаем обработчик для сообщения пользователя
    connect(&generator,
            &LLMCore::EmbeddingsGenerator::messageEmbeddingGenerated,
            this,
            [query, &storage, this](const QString &message, const QVector<float> &embedding) {
                qDebug() << "Generated embedding for query:" << query.left(50) << "...";

                // Ищем похожий код
                auto results = storage.findSimilarCode(embedding, 0.65f, 5);

                // Формируем ответ на основе найденного кода
                QString response;
                for (const auto &result : results) {
                    response += QString("%1 %2 %3 similarity:%4 %5")
                                    .arg(result.filePath)
                                    .arg(result.startLine)
                                    .arg(result.endLine)
                                    .arg(result.similarity, 0, 'f', 4)
                                    .arg(result.content);
                }

                qDebug() << "Response:" << response;

                m_clientInterface->sendMessage(message, response);

                // sendMessage(query, response)
                // Здесь можно отправить response в чат или показать пользователю
            });

    // Подключаем обработчик ошибок
    connect(&generator, &LLMCore::EmbeddingsGenerator::error, this, [](const QString &error) {
        qDebug() << "Error generating embedding for query:" << error;
    });

    // Генерируем эмбеддинг для запроса
    generator.generateEmbedding(query);
}

void ChatRootView::generateColors()
{
    QColor baseColor = backgroundColor();
    bool isDarkTheme = baseColor.lightness() < 128;

    if (isDarkTheme) {
        m_primaryColor = generateColor(baseColor, 0.1, 1.2, 1.4);
        m_secondaryColor = generateColor(baseColor, -0.1, 1.1, 1.2);
        m_codeColor = generateColor(baseColor, 0.05, 0.8, 1.1);
    } else {
        m_primaryColor = generateColor(baseColor, 0.05, 1.05, 1.1);
        m_secondaryColor = generateColor(baseColor, -0.05, 1.1, 1.2);
        m_codeColor = generateColor(baseColor, 0.02, 0.95, 1.05);
    }
}

QColor ChatRootView::generateColor(const QColor &baseColor,
                                   float hueShift,
                                   float saturationMod,
                                   float lightnessMod)
{
    float h, s, l, a;
    baseColor.getHslF(&h, &s, &l, &a);
    bool isDarkTheme = l < 0.5;

    h = fmod(h + hueShift + 1.0, 1.0);

    s = qBound(0.0f, s * saturationMod, 1.0f);

    if (isDarkTheme) {
        l = qBound(0.0f, l * lightnessMod, 1.0f);
    } else {
        l = qBound(0.0f, l / lightnessMod, 1.0f);
    }

    h = qBound(0.0f, h, 1.0f);
    s = qBound(0.0f, s, 1.0f);
    l = qBound(0.0f, l, 1.0f);
    a = qBound(0.0f, a, 1.0f);

    return QColor::fromHslF(h, s, l, a);
}

QString ChatRootView::currentTemplate() const
{
    auto &settings = Settings::generalSettings();
    return settings.chatModelName();
}

QColor ChatRootView::primaryColor() const
{
    return m_primaryColor;
}

QColor ChatRootView::secondaryColor() const
{
    return m_secondaryColor;
}

QColor ChatRootView::codeColor() const
{
    return m_codeColor;
}

bool ChatRootView::isDarkTheme() const
{
    float h, s, l, a;
    m_primaryColor.getHslF(&h, &s, &l, &a);
    return l < 0.5;
}

bool ChatRootView::isAnswering() const
{
    return m_isAnswering;
}

void ChatRootView::setIsAnswering(bool state)
{
    m_isAnswering = state;
    emit isAnsweringChanged();
}

} // namespace QodeAssist::Chat
