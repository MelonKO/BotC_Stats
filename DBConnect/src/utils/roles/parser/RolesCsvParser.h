#pragma once
#include <qmap.h>
#include <QString>
#include <QVector>

namespace botc::utils
{
    struct RoleRecord
    {
        QString name;
        QString alignment;
        QString roleType;
        QString description;
        // "ru" -> {name, description}
        QMap<QString, QPair<QString, QString>> translations;
        QStringList extraColumns; // unknown columns, kept so that no data is lost
    };

    struct ParseResult
    {
        QList<RoleRecord> records;
        QStringList headers;   // original headers from the file
        QStringList languages; // detected languages: "ru", "de", ...
        QStringList errors;    // validation errors, already translated for the UI
        // CSV line of the matching entry in errors (1-based, header is line 1),
        // -1 for an error that belongs to no single line. Lets the view mark
        // broken rows without parsing the localized message text.
        QVector<int> errorLines;
        bool success = false;
    };

    class RolesCsvParser
    {
    public:
        // Allowed values for validation
        static const QStringList VALID_ALIGNMENTS;
        static const QStringList VALID_ROLE_TYPES;

        static ParseResult parse(const QString& filePath);

    private:
        static QMap<QString, int> mapHeaders(const QStringList& headers, QStringList& outLanguages);
        static QStringList validateRecord(const RoleRecord& record, int rowIndex);
    };
} // botc::utils
