#pragma once
#include <qmap.h>
#include <QString>

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
        QStringList extraColumns; // неизвестные колонки — не теряем данные
    };

    struct ParseResult
    {
        QList<RoleRecord> records;
        QStringList headers;   // оригинальные заголовки из файла
        QStringList languages; // обнаруженные языки: "ru", "de", ...
        QStringList errors;    // ошибки валидации: "Строка 3: поле name пустое"
        bool success = false;
    };

    class RolesCsvParser
    {
    public:
        // Допустимые значения для валидации
        static const QStringList VALID_ALIGNMENTS;
        static const QStringList VALID_ROLE_TYPES;

        ParseResult parse(const QString& filePath);

    private:
        QStringList parseCsvLine(const QString& line);
        QMap<QString, int> mapHeaders(const QStringList& headers, QStringList& outLanguages);
        QStringList validateRecord(const RoleRecord& record, int rowIndex);
    };
} // botc::utils
