#include "RolesCsvParser.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QSet>

#include "../../csv/CsvParser.h"

namespace botc::utils
{
    const QStringList RolesCsvParser::VALID_ALIGNMENTS = {
        "good", "evil", "neutral"
    };

    const QStringList RolesCsvParser::VALID_ROLE_TYPES = {
        "Townsfolk", "Outsider", "Minion", "Demon", "Traveller"
    };

    ParseResult RolesCsvParser::parse(const QString& filePath)
    {
        ParseResult result;

        CsvParser parser(filePath);
        if (auto [bSuccess, errors] = parser.openFile(); !bSuccess)
        {
            result.errors = errors;
            return result;
        }

        const QStringList headers = parser.parseNextLine();
        result.headers            = headers;

        QStringList detectedLanguages;
        const QMap<QString, int> headerToIndexMap = mapHeaders(headers, detectedLanguages);
        result.languages                          = detectedLanguages;

        auto createCellReader = [&headerToIndexMap](QStringList in_row) -> auto
        {
            return [&headerToIndexMap, &in_row](const QString& col) -> QString
            {
                const int idx = headerToIndexMap.value(col, -1);
                if (idx < 0 || idx >= in_row.size()) return {};
                return in_row[idx].trimmed();
            };
        };

        // Check required columns
        for (const QString& col : {"name", "alignment", "role_type", "description"})
        {
            if (!headerToIndexMap.contains(col))
            {
                result.errors << QCoreApplication::translate("RolesCsvParser",
                                                             "A required column is missing: '%1'").arg(col);
                result.errorLines << -1;
            }
        }
        if (!result.errors.isEmpty()) return result;

        // Read the rows
        int rowIndex = 2; // 1-based, the first line is the header
        while (parser.isCanReadNext())
        {
            QStringList row = parser.parseNextLine();
            if (row.isEmpty())
            {
                ++rowIndex;
                continue;
            }

            RoleRecord record;
            auto readCell = createCellReader(row);

            record.name        = readCell("name");
            record.alignment   = readCell("alignment");
            record.roleType    = readCell("role_type");
            record.description = readCell("description");

            // Translations: look for columns named "<lang>_name", "<lang>_description"
            for (const QString& lang : detectedLanguages)
            {
                record.translations[lang].first  = readCell(lang + "_name");
                record.translations[lang].second = readCell(lang + "_description");
            }

            // Validation
            const int errorsBefore = result.errors.size();
            result.errors << validateRecord(record, rowIndex);
            for (int errorIndex = errorsBefore; errorIndex < result.errors.size(); ++errorIndex)
                result.errorLines << rowIndex;

            result.records << record;
            ++rowIndex;
        }

        result.success = result.errors.isEmpty();
        return result;
    }

    QMap<QString, int> RolesCsvParser::mapHeaders(const QStringList& headers, QStringList& outLanguages)
    {
        QMap<QString, int> index;
        QSet<QString> langs;

        // Pattern: "<lang>_name" or "<lang>_description"
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

    QStringList RolesCsvParser::validateRecord(const RoleRecord& record, const int rowIndex)
    {
        QStringList errors;
        auto err = [&](const QString& msg)
        {
            errors << QCoreApplication::translate("RolesCsvParser", "Line %1: %2").arg(rowIndex).arg(msg);
        };

        if (record.name.isEmpty())
            err(QCoreApplication::translate("RolesCsvParser", "The 'name' field cannot be empty."));

        if (!VALID_ALIGNMENTS.contains(record.alignment))
            err(QCoreApplication::translate("RolesCsvParser", "Invalid \"alignment\" value '%1'. "
                                                              "Valid values: %2")
                .arg(record.alignment, VALID_ALIGNMENTS.join(", ")));

        if (!VALID_ROLE_TYPES.contains(record.roleType))
            err(QCoreApplication::translate("RolesCsvParser", "Invalid \"role_type\" value '%1'. "
                                                              "Valid values: %2")
                .arg(record.roleType, VALID_ROLE_TYPES.join(", ")));

        if (record.description.isEmpty())
            err(QCoreApplication::translate("RolesCsvParser", "The 'description' field cannot be empty."));

        return errors;
    }
} // botc::utils
