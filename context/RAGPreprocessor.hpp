#include <QRegularExpression>
#include <QString>

#include "Logger.hpp"

namespace QodeAssist::Context {

class RAGPreprocessor
{
public:
    static const QRegularExpression &getLicenseRegex()
    {
        static const QRegularExpression regex(
            R"((/\*[^*]*\*+(?:[^/*][^*]*\*+)*/)|//[^\n]*(?:\n|$))",
            QRegularExpression::MultilineOption);
        return regex;
    }

    static const QRegularExpression &getClassRegex()
    {
        static const QRegularExpression regex(
            R"((?:template\s*<[^>]*>\s*)?(?:class|struct)\s+(\w+)\s*(?:final\s*)?(?::\s*(?:public|protected|private)\s+\w+(?:\s*,\s*(?:public|protected|private)\s+\w+)*\s*)?{)");
        return regex;
    }

    static QString preprocessCode(const QString &code)
    {
        if (code.isEmpty()) {
            return QString();
        }

        try {
            // Прямое разделение без промежуточной копии
            QStringList lines = code.split('\n', Qt::SkipEmptyParts);
            return processLines(lines);
        } catch (const std::exception &e) {
            LOG_MESSAGE(QString("Error preprocessing code: %1").arg(e.what()));
            return code; // Возвращаем оригинальный код в случае ошибки
        }
    }

private:
    static QString processLines(const QStringList &lines)
    {
        const int estimatedAvgLength = 80; // Примерная средняя длина строки
        QString result;
        result.reserve(lines.size() * estimatedAvgLength);

        for (const QString &line : lines) {
            const QString trimmed = line.trimmed();
            if (!trimmed.isEmpty()) {
                result += trimmed;
                result += QLatin1Char('\n');
            }
        }

        // Убираем последний перенос строки, если он есть
        if (result.endsWith('\n')) {
            result.chop(1);
        }

        return result;
    }
};

} // namespace QodeAssist::Context
