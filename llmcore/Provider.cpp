#include "Provider.hpp"

namespace QodeAssist::LLMCore {

Provider::Provider(QObject *parent)
    : QObject(parent)
    , m_httpClient(std::make_unique<HttpClient>())
{
    connect(m_httpClient.get(), &HttpClient::dataReceived, this, &Provider::onDataReceived);
    connect(m_httpClient.get(), &HttpClient::requestFinished, this, &Provider::onRequestFinished);
}

HttpClient *Provider::httpClient() const
{
    return m_httpClient.get();
}

} // namespace QodeAssist::LLMCore
