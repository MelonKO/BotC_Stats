#include "CsvParser.h"

#include <QFile>

namespace botc::utils
{
    CsvParser::CsvParser(const QString& in_filePath) :
        m_file(in_filePath)
    {
    }

    QPair<bool, QStringList> CsvParser::openFile()
    {
        QPair<bool, QStringList> result;
        if (!m_file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            result.first = false;
            result.second << QString("Can't open file: %1")
                             .arg(m_file.fileName())
                             .arg(m_file.errorString());
            return result;
        }

        m_textStream.setDevice(&m_file);
        m_textStream.setEncoding(QStringConverter::Utf8);

        if (m_textStream.atEnd())
        {
            result.first = false;
            result.second << QString("File is empty");
            return result;
        }

        result.first = true;
        return result;
    }

    QStringList CsvParser::parseNextLine()
    {
        return parseCsvLine(m_textStream.readLine().trimmed());
    }

    QStringList CsvParser::parseCsvLine(const QString& in_line)
    {
        QStringList fields;
        QString current;
        bool inQuotes = false;

        for (int i = 0; i < in_line.size(); ++i)
        {
            QChar c = in_line[i];
            if (inQuotes)
            {
                if (c == '"')
                {
                    if (i + 1 < in_line.size() && in_line[i + 1] == '"')
                    {
                        current += '"';
                        ++i;
                    }
                    else
                    {
                        inQuotes = false;
                    }
                }
                else
                {
                    current += c;
                }
            }
            else
            {
                if (c == '"') inQuotes = true;
                else if (c == ',')
                {
                    fields << current;
                    current.clear();
                }
                else current += c;
            }
        }
        if (!current.isEmpty())
        {
            fields << current;
        }
        return fields;
    }

    bool CsvParser::isCanReadNext() const
    {
        return !m_textStream.atEnd();
    }
} // botc::utils
