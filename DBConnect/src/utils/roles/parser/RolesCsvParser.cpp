#include "RolesCsvParser.h"

#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <qtextstream.h>

namespace botc::utils
{
    const QStringList RolesCsvParser::VALID_ALIGNMENTS = {
        "good", "evil", "neutral"
    };

    const QStringList RolesCsvParser::VALID_ROLE_TYPES = {
        "Townsfolk", "Outsider", "Minion", "Demon", "Traveler"
    };

    ParseResult RolesCsvParser::parse(const QString& filePath)
    {
        ParseResult result;

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            result.errors << QString("Не удалось открыть файл: %1").arg(filePath);
            return result;
        }

        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);

        // Читаем заголовок
        if (stream.atEnd())
        {
            result.errors << "Файл пустой.";
            return result;
        }

        const QStringList headers = parseCsvLine(stream.readLine());
        result.headers            = headers;

        QStringList detectedLanguages;
        const QMap<QString, int> colIndex = mapHeaders(headers, detectedLanguages);
        result.languages                  = detectedLanguages;

        // Проверяем обязательные колонки
        const QStringList required = {"name", "alignment", "role_type", "description"};
        for (const QString& col : required)
        {
            if (!colIndex.contains(col))
            {
                result.errors << QString("Отсутствует обязательная колонка: '%1'").arg(col);
            }
        }
        if (!result.errors.isEmpty()) return result;

        // Читаем строки
        int rowIndex = 2; // нумерация с 1, первая строка — заголовок
        while (!stream.atEnd())
        {
            const QString line = stream.readLine();
            if (line.trimmed().isEmpty())
            {
                ++rowIndex;
                continue;
            }

            const QStringList cells = parseCsvLine(line);

            RoleRecord record;
            auto cell = [&](const QString& col) -> QString
            {
                int idx = colIndex.value(col, -1);
                if (idx < 0 || idx >= cells.size()) return {};
                return cells[idx].trimmed();
            };

            record.name        = cell("name");
            record.alignment   = cell("alignment");
            record.roleType    = cell("role_type");
            record.description = cell("description");

            // Переводы: ищем колонки вида "<lang>_name", "<lang>_description"
            for (const QString& lang : detectedLanguages)
            {
                record.translations[lang].first  = cell(lang + "_name");
                record.translations[lang].second = cell(lang + "_description");
            }

            // Валидация
            const QStringList rowErrors = validateRecord(record, rowIndex);
            result.errors << rowErrors;

            result.records << record;
            ++rowIndex;
        }

        result.success = result.errors.isEmpty();
        return result;
    }

    QStringList RolesCsvParser::parseCsvLine(const QString& line)
    {
        QStringList fields;
        QString current;
        bool inQuotes = false;

        for (int i = 0; i < line.size(); ++i)
        {
            QChar c = line[i];

            if (inQuotes)
            {
                if (c == '"')
                {
                    // Экранированная кавычка "" внутри поля
                    if (i + 1 < line.size() && line[i + 1] == '"')
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
                if (c == '"')
                {
                    inQuotes = true;
                }
                else if (c == ',')
                {
                    fields << current;
                    current.clear();
                }
                else
                {
                    current += c;
                }
            }
        }
        fields << current;
        return fields;
    }

    QMap<QString, int> RolesCsvParser::mapHeaders(const QStringList& headers, QStringList& outLanguages)
    {
        QMap<QString, int> index;
        QSet<QString> langs;

        // Паттерн: "<lang>_name" или "<lang>_description"
        static const QRegularExpression langPattern(R"(^([a-z]{2,5})_(name|description)$)");

        for (int i = 0; i < headers.size(); ++i)
        {
            const QString h = headers[i].trimmed().toLower();
            index[h]        = i;

            auto match = langPattern.match(h);
            if (match.hasMatch())
            {
                langs.insert(match.captured(1)); // "ru", "de", ...
            }
        }

        outLanguages = QStringList(langs.begin(), langs.end());
        outLanguages.sort();
        return index;
    }

    QStringList RolesCsvParser::validateRecord(const RoleRecord& record, int rowIndex)
    {
        QStringList errors;
        auto err = [&](const QString& msg)
        {
            errors << QString("Строка %1: %2").arg(rowIndex).arg(msg);
        };

        if (record.name.isEmpty())
            err("поле 'name' не может быть пустым.");

        if (!VALID_ALIGNMENTS.contains(record.alignment))
            err(QString("недопустимое значение alignment='%1'. "
                "Допустимые: %2").arg(record.alignment, VALID_ALIGNMENTS.join(", ")));

        if (!VALID_ROLE_TYPES.contains(record.roleType))
            err(QString("недопустимое значение role_type='%1'. "
                "Допустимые: %2").arg(record.roleType, VALID_ROLE_TYPES.join(", ")));

        if (record.description.isEmpty())
            err("поле 'description' не может быть пустым.");

        return errors;
    }
} // botc::utils
