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

#include "OllamaProvider.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QProcess>

#include "PromptTemplateManager.hpp"
#include "QodeAssistSettings.hpp"

namespace QodeAssist::Providers {

OllamaProvider::OllamaProvider() {}

QString OllamaProvider::name() const
{
    return "Ollama";
}

QString OllamaProvider::url() const
{
    return "http://localhost:11434";
}

QString OllamaProvider::completionEndpoint() const
{
    return "/api/generate";
}

void OllamaProvider::prepareRequest(QJsonObject &request)
{
    auto currentTemplate = PromptTemplateManager::instance().getCurrentTemplate();

    QJsonObject options;
    options["num_predict"] = settings().maxTokens();
    options["keep_alive"] = settings().ollamaLivetime();
    options["temperature"] = settings().temperature();
    options["stop"] = QJsonArray::fromStringList(currentTemplate->stopWords());
    if (settings().useTopP())
        options["top_p"] = settings().topP();
    if (settings().useTopK())
        options["top_k"] = settings().topK();
    if (settings().useFrequencyPenalty())
        options["frequency_penalty"] = settings().frequencyPenalty();
    if (settings().usePresencePenalty())
        options["presence_penalty"] = settings().presencePenalty();
    request["options"] = options;
}

bool OllamaProvider::handleResponse(QNetworkReply *reply, QString &accumulatedResponse)
{
    bool isComplete = false;
    while (reply->canReadLine()) {
        QByteArray line = reply->readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        QJsonDocument jsonResponse = QJsonDocument::fromJson(line);
        if (jsonResponse.isNull()) {
            qWarning() << "Invalid JSON response from Ollama:" << line;
            continue;
        }
        QJsonObject responseObj = jsonResponse.object();
        if (responseObj.contains("response")) {
            QString completion = responseObj["response"].toString();

            accumulatedResponse += completion;
        }
        if (responseObj["done"].toBool()) {
            isComplete = true;
            break;
        }
    }
    return isComplete;
}

QList<QString> OllamaProvider::getInstalledModels(const Utils::Environment &env)
{
    QProcess process;
    process.setEnvironment(env.toStringList());
    QString ollamaConsoleName;
#ifdef Q_OS_WIN
    ollamaConsoleName = "ollama.exe";
#else
    ollamaConsoleName = "ollama";
#endif

    auto ollamaPath = env.searchInPath(ollamaConsoleName).toString();

    if (!QFileInfo::exists(ollamaPath)) {
        qWarning() << "Ollama executable not found at" << ollamaPath;
        return {};
    }

    process.start(ollamaPath, QStringList() << "list");
    if (!process.waitForStarted()) {
        qWarning() << "Failed to start Ollama process:" << process.errorString();
        return {};
    }

    if (!process.waitForFinished()) {
        qWarning() << "Ollama process did not finish:" << process.errorString();
        return {};
    }

    QStringList models;
    if (process.exitCode() == 0) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);

        for (int i = 1; i < lines.size(); ++i) {
            QString line = lines[i].trimmed();
            if (!line.isEmpty()) {
                QString modelName = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)
                                        .first();
                models.append(modelName);
            }
        }
    } else {
        qWarning() << "Error running 'ollama list':" << process.errorString();
    }

    return models;
}

} // namespace QodeAssist::Providers
