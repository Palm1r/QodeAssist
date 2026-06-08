// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "TokenUtils.hpp"

#include <QFileInfo>
#include <QImageReader>
#include <QSet>
#include <QSize>

#include <algorithm>
#include <cmath>

namespace QodeAssist::Context {

int TokenUtils::estimateTokens(const QString &text)
{
    if (text.isEmpty()) {
        return 0;
    }

    // TODO: need to improve
    return text.length() / 4;
}

bool TokenUtils::isImageFilePath(const QString &filePath)
{
    static const QSet<QString> imageExtensions = {"png", "jpg", "jpeg", "gif", "webp", "bmp"};
    return imageExtensions.contains(QFileInfo(filePath).suffix().toLower());
}

int TokenUtils::estimateImageAttachmentTokens(const QString &filePath)
{
    QImageReader reader(filePath);
    QSize size = reader.size();
    if (!size.isValid() || size.isEmpty())
        return 1500;

    double w = size.width();
    double h = size.height();

    const double longSide = std::max(w, h);
    if (longSide > 2048.0) {
        const double s = 2048.0 / longSide;
        w *= s;
        h *= s;
    }

    const double shortSide = std::min(w, h);
    if (shortSide > 768.0) {
        const double s = 768.0 / shortSide;
        w *= s;
        h *= s;
    }

    const int tilesW = static_cast<int>(std::ceil(w / 512.0));
    const int tilesH = static_cast<int>(std::ceil(h / 512.0));
    const int tiles = std::max(1, tilesW * tilesH);

    return 85 + tiles * 170;
}

int TokenUtils::estimateFileTokens(const Context::ContentFile &file)
{
    if (isImageFilePath(file.filename))
        return estimateImageAttachmentTokens(QString());

    int total = 0;

    total += estimateTokens(file.filename);
    total += estimateTokens(file.content);
    total += 5;

    return total;
}

int TokenUtils::estimateFilesTokens(const QList<Context::ContentFile> &files)
{
    int total = 0;
    for (const auto &file : files) {
        total += estimateFileTokens(file);
    }
    return total;
}

} // namespace QodeAssist::Context
