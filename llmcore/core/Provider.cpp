#include "Provider.hpp"

#include <llmcore/network/BaseClient.hpp>

namespace QodeAssist::LLMCore {

Provider::Provider(QObject *parent)
    : QObject(parent)
{
}

void Provider::connectClientSignals(BaseClient *client)
{
    connect(
        client, &BaseClient::partialResponseReceived, this, &Provider::partialResponseReceived);
    connect(client, &BaseClient::fullResponseReceived, this, &Provider::fullResponseReceived);
    connect(client, &BaseClient::requestFailed, this, &Provider::requestFailed);
    connect(client, &BaseClient::toolExecutionStarted, this, &Provider::toolExecutionStarted);
    connect(
        client, &BaseClient::toolExecutionCompleted, this, &Provider::toolExecutionCompleted);
    connect(client, &BaseClient::continuationStarted, this, &Provider::continuationStarted);
    connect(client, &BaseClient::thinkingBlockReceived, this, &Provider::thinkingBlockReceived);
    connect(
        client,
        &BaseClient::redactedThinkingBlockReceived,
        this,
        &Provider::redactedThinkingBlockReceived);
}

} // namespace QodeAssist::LLMCore
