// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <gtest/gtest.h>

#include <ErrorInfo.hpp>

using namespace QodeAssist;

TEST(ErrorInfoTest, MakeErrorPopulatesFields)
{
    const ErrorInfo e = makeError(ErrorCategory::Tool, QStringLiteral("boom"), QStringLiteral("detail"));
    EXPECT_EQ(e.category, ErrorCategory::Tool);
    EXPECT_EQ(e.message, QStringLiteral("boom"));
    EXPECT_EQ(e.providerDetail, QStringLiteral("detail"));
    EXPECT_FALSE(e.isEmpty());
}

TEST(ErrorInfoTest, DefaultIsEmpty)
{
    ErrorInfo e;
    EXPECT_TRUE(e.isEmpty());
    EXPECT_EQ(e.category, ErrorCategory::Provider);
}

TEST(ErrorInfoTest, EmptyMessageIsEmptyRegardlessOfCategory)
{
    const ErrorInfo e = makeError(ErrorCategory::Auth, QString());
    EXPECT_TRUE(e.isEmpty());
}

TEST(ErrorInfoTest, CategorizesHttp401AsAuth)
{
    EXPECT_EQ(categorizeProviderError(QStringLiteral("HTTP 401 Unauthorized")), ErrorCategory::Auth);
}

TEST(ErrorInfoTest, CategorizesForbiddenAsAuth)
{
    EXPECT_EQ(categorizeProviderError(QStringLiteral("403 Forbidden")), ErrorCategory::Auth);
}

TEST(ErrorInfoTest, CategorizesApiKeyAsAuth)
{
    EXPECT_EQ(categorizeProviderError(QStringLiteral("invalid api key supplied")), ErrorCategory::Auth);
}

TEST(ErrorInfoTest, CategorizesAuthenticationAsAuth)
{
    EXPECT_EQ(categorizeProviderError(QStringLiteral("Authentication failed")), ErrorCategory::Auth);
}

TEST(ErrorInfoTest, AuthMatchIsCaseInsensitive)
{
    EXPECT_EQ(categorizeProviderError(QStringLiteral("UNAUTHORIZED")), ErrorCategory::Auth);
}

TEST(ErrorInfoTest, CategorizesTimeoutAsNetwork)
{
    EXPECT_EQ(categorizeProviderError(QStringLiteral("request timed out")), ErrorCategory::Network);
}

TEST(ErrorInfoTest, CategorizesConnectionRefusedAsNetwork)
{
    EXPECT_EQ(categorizeProviderError(QStringLiteral("Connection refused")), ErrorCategory::Network);
}

TEST(ErrorInfoTest, CategorizesDnsFailureAsNetwork)
{
    EXPECT_EQ(
        categorizeProviderError(QStringLiteral("could not resolve host")), ErrorCategory::Network);
}

TEST(ErrorInfoTest, CategorizesSslAsNetwork)
{
    EXPECT_EQ(categorizeProviderError(QStringLiteral("SSL handshake error")), ErrorCategory::Network);
}

TEST(ErrorInfoTest, UnrecognizedErrorFallsBackToProvider)
{
    EXPECT_EQ(
        categorizeProviderError(QStringLiteral("model produced an internal error")),
        ErrorCategory::Provider);
}
