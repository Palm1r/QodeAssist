#include "Provider.hpp"

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

} // namespace QodeAssist::LLMCore
