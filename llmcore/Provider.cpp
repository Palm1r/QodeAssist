#include "Provider.hpp"

#include <QJsonDocument>

namespace QodeAssist::LLMCore {

Provider::Provider(QObject *parent)
    : QObject(parent)
    , m_httpClient(new HttpClient(this))
{
    connect(m_httpClient, &HttpClient::dataReceived, this, &Provider::onDataReceived);
    connect(m_httpClient, &HttpClient::requestFinished, this, &Provider::onRequestFinished);
}

HttpClient *Provider::httpClient() const
{
    return m_httpClient;
}

QJsonObject Provider::parseEventLine(const QString &line)
{
    if (!line.startsWith("data: "))
        return QJsonObject();

    QString jsonStr = line.mid(6);

    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    return doc.object();
}

} // namespace QodeAssist::LLMCore
