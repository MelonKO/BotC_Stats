#pragma once

#include <QFile>
#include <QTextStream>

namespace botc::utils
{
    class CsvParser
    {
    public:
        explicit CsvParser(const QString& in_filePath);

        [[nodiscard]] QPair<bool, QStringList> openFile();
        [[nodiscard]] QStringList parseNextLine();
        [[nodiscard]] static QStringList parseCsvLine(const QString& in_line);
        [[nodiscard]] bool isCanReadNext() const;

    private:
        QFile m_file;
        QTextStream m_textStream;
    };
} // botc::utils
