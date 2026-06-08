// Copyright (C) 2025 Povilas Kanapickas
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <gtest/gtest.h>
#include <QGuiApplication>
#include <QtGlobal>

void silenceQtWarningNoise(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (msg.startsWith("SOFT ASSERT") || msg.startsWith("Accessing MimeDatabase")) {
        return;
    }
    qt_message_output(type, context, msg);
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(silenceQtWarningNoise);
    QGuiApplication application(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
